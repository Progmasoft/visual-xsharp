// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

func fuzzConfiguration(currentHost host) (string, error) {
	switch currentHost.kind {
	case hostWindows:
		return "fuzz-windows", nil
	case hostMacOS:
		return "fuzz-macos", nil
	case hostLinux:
		return "fuzz-linux", nil
	default:
		return "", errors.New("coverage-guided fuzzing requires a supported host")
	}
}

func macOSFuzzerRuntime(root string) (string, error) {
	if root == "" {
		return "", errors.New("LLVM_ROOT is required to locate the macOS libFuzzer runtime")
	}
	matches, err := filepath.Glob(filepath.Join(root, "lib", "clang", "*", "lib", "darwin", "libclang_rt.fuzzer_osx.a"))
	if err != nil || len(matches) != 1 {
		return "", fmt.Errorf("expected exactly one Homebrew LLVM macOS libFuzzer runtime under %q, found %d", root, len(matches))
	}
	return matches[0], nil
}

// Smoke programs own main; only the four campaign drivers may link libFuzzer's
// main. Sharing one global fuzz profile with both groups duplicates main on
// Linux, where -fsanitize=fuzzer pulls the driver in unconditionally.
func fuzzBuildArguments(configuration, sanitizerConfiguration, macRuntime string) ([]string, []string) {
	smoke := []string{"build"}
	campaign := []string{"build", "--config=" + configuration}
	if sanitizerConfiguration != "" {
		smoke = append(smoke, "--config="+sanitizerConfiguration)
		campaign = append(campaign, "--config="+sanitizerConfiguration)
	}
	if macRuntime != "" {
		campaign = append(campaign, "--linkopt="+macRuntime)
	}
	smoke = append(smoke, "//Compiler/Fuzzing:wire_fuzz_smoke", "//Compiler/Fuzzing:source_fuzz_smoke")
	campaign = append(campaign,
		"//Compiler/Fuzzing:wire_fuzzer",
		"//Compiler/Fuzzing:lexer_fuzzer",
		"//Compiler/Fuzzing:parser_fuzzer",
		"//Compiler/Fuzzing:source_llvm_fuzzer")
	return smoke, campaign
}

func runFuzzCampaign(repository string, currentHost host, runner commandRunner, stress bool, asan bool) error {
	configuration, err := fuzzConfiguration(currentHost)
	if err != nil {
		return err
	}
	frontendLibrary, err := buildFrontendLibrary(repository, runner)
	if err != nil {
		return err
	}
	bazel, err := findBazel(runner)
	if err != nil {
		return err
	}
	temporaryRoot := os.Getenv("RUNNER_TEMP")
	if temporaryRoot == "" {
		temporaryRoot = os.TempDir()
	}
	work, err := os.MkdirTemp(temporaryRoot, "vxs-fuzz-")
	if err != nil {
		return fmt.Errorf("could not create fuzz work directory: %w", err)
	}
	persistentCorpus := os.Getenv("VXS_FUZZ_CORPUS")
	corpusRoot := persistentCorpus
	if corpusRoot == "" {
		corpusRoot = filepath.Join(work, "corpus")
	}
	artifacts := filepath.Join(work, "artifacts")
	if err := os.Mkdir(artifacts, 0o700); err != nil {
		return fmt.Errorf("could not create fuzz artifact directory %q: %w", work, err)
	}
	stageNames := []string{"wire", "lexer", "parser", "source"}
	for _, stage := range stageNames {
		corpus := filepath.Join(corpusRoot, stage)
		if err := os.MkdirAll(corpus, 0o700); err != nil {
			return fmt.Errorf("could not prepare %s corpus; preserved %q: %w", stage, work, err)
		}
		seedName := stage
		if stage == "source" {
			seedName = "source"
		}
		if err := syncSeedCorpus(filepath.Join(repository, "Compiler", "Fuzzing", "Corpus", seedName), corpus); err != nil {
			return fmt.Errorf("could not synchronize the versioned %s seed corpus: %w", stage, err)
		}
	}
	wireGenerated := filepath.Join(work, "generated-wire-corpus")
	smoke := filepath.Join(repository, "bazel-bin", "Compiler", "Fuzzing", "wire_fuzz_smoke"+currentHost.executable)
	sanitizerConfiguration := ""
	selectedEnvironment := []string(nil)
	if asan {
		selected, err := selectSanitizer(currentHost, "address")
		if err != nil {
			return err
		}
		sanitizerConfiguration = selected.config
		selectedEnvironment, err = sanitizerEnvironment(currentHost, selected, runner)
		if err != nil {
			return err
		}
	}
	macRuntime := ""
	if currentHost.kind == hostMacOS {
		runtime, err := macOSFuzzerRuntime(os.Getenv("LLVM_ROOT"))
		if err != nil {
			return fmt.Errorf("could not locate macOS libFuzzer runtime; preserved %q: %w", work, err)
		}
		macRuntime = runtime
	}
	smokeArguments, campaignArguments := fuzzBuildArguments(configuration, sanitizerConfiguration, macRuntime)
	if err := runner.Run(repository, nil, bazel, smokeArguments...); err != nil {
		return fmt.Errorf("could not build standalone fuzz smoke targets; preserved %q: %w", work, err)
	}
	if err := runner.Run(repository, selectedEnvironment, smoke, "-Write-Corpus", wireGenerated); err != nil {
		return fmt.Errorf("could not export valid wire seeds; preserved %q: %w", work, err)
	}
	if err := syncSeedCorpus(wireGenerated, filepath.Join(corpusRoot, "wire")); err != nil {
		return fmt.Errorf("could not add generated wire seeds to the persistent corpus: %w", err)
	}
	smokeSource := filepath.Join(repository, "bazel-bin", "Compiler", "Fuzzing", "source_fuzz_smoke"+currentHost.executable)
	if err := copyFile(frontendLibrary, filepath.Join(filepath.Dir(smokeSource), filepath.Base(frontendLibrary)), 0o755); err != nil {
		return fmt.Errorf("could not stage Haskell frontend for source-fuzz smoke: %w", err)
	}
	if err := runner.Run(repository, selectedEnvironment, smokeSource); err != nil {
		return fmt.Errorf("source-to-LLVM differential smoke failed; preserved %q: %w", work, err)
	}
	// Run smoke tests before changing Bazel's instrumentation configuration and
	// staging the campaign binaries into the same host output tree.
	if err := runner.Run(repository, nil, bazel, campaignArguments...); err != nil {
		return fmt.Errorf("could not build instrumented fuzz targets; preserved %q: %w", work, err)
	}
	duration := 30
	if stress {
		duration = 900
	}
	targets := []struct {
		binary    string
		corpus    string
		maxLength string
		rssLimit  string
	}{
		{"wire_fuzzer", "wire", "16384", "768"},
		{"lexer_fuzzer", "lexer", "65536", "1024"},
		{"parser_fuzzer", "parser", "65536", "1536"},
		{"source_llvm_fuzzer", "source", "65536", "4096"},
	}
	for _, target := range targets {
		fuzzer := filepath.Join(repository, "bazel-bin", "Compiler", "Fuzzing", target.binary+currentHost.executable)
		if strings.Contains(target.binary, "lexer") || strings.Contains(target.binary, "parser") || strings.Contains(target.binary, "source_llvm") {
			if err := copyFile(frontendLibrary, filepath.Join(filepath.Dir(fuzzer), filepath.Base(frontendLibrary)), 0o755); err != nil {
				return fmt.Errorf("could not stage frontend for %s; preserved %q: %w", target.binary, work, err)
			}
		}
		campaignArtifacts := filepath.Join(artifacts, target.binary)
		if err := os.MkdirAll(campaignArtifacts, 0o700); err != nil {
			return fmt.Errorf("could not create %s artifact directory: %w", target.binary, err)
		}
		arguments := []string{
			filepath.Join(corpusRoot, target.corpus),
			"-max_total_time=" + strconv.Itoa(duration),
			"-max_len=" + target.maxLength,
			"-timeout=30",
			"-rss_limit_mb=" + target.rssLimit,
			"-use_value_profile=1",
			"-verbosity=0",
			"-print_final_stats=1",
			"-artifact_prefix=" + campaignArtifacts + string(os.PathSeparator),
		}
		if err := runner.Run(repository, selectedEnvironment, fuzzer, arguments...); err != nil {
			return fmt.Errorf("%s campaign failed; corpus and crash artifacts preserved in %q: %w", target.binary, work, err)
		}
		fmt.Printf("%s campaign completed (%d seconds; RSS <= %s MiB).\n", target.binary, duration, target.rssLimit)
	}
	if persistentCorpus != "" {
		if err := removeSuccessfulFuzzWork(temporaryRoot, work); err != nil {
			return err
		}
		fmt.Printf("Coverage corpus persisted in %s.\n", persistentCorpus)
		return nil
	}
	// Only the exact directory returned by MkdirTemp may be removed. On a
	// failed campaign the same directory remains for replay and minimization.
	if err := removeSuccessfulFuzzWork(temporaryRoot, work); err != nil {
		return err
	}
	fmt.Println("All coverage-guided compiler fuzz targets completed without a reported failure.")
	return nil
}

func syncSeedCorpus(source string, destination string) error {
	entries, err := os.ReadDir(source)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	for _, entry := range entries {
		if entry.IsDir() || entry.Type()&os.ModeSymlink != 0 {
			continue
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		if !info.Mode().IsRegular() {
			continue
		}
		target := filepath.Join(destination, entry.Name())
		if _, err := os.Stat(target); err == nil {
			continue
		} else if !os.IsNotExist(err) {
			return err
		}
		if err := copyFile(filepath.Join(source, entry.Name()), target, 0o600); err != nil {
			return err
		}
	}
	return nil
}

func removeSuccessfulFuzzWork(temporaryRoot string, work string) error {
	root, err := filepath.Abs(temporaryRoot)
	if err != nil {
		return err
	}
	target, err := filepath.Abs(work)
	if err != nil {
		return err
	}
	relative, err := filepath.Rel(root, target)
	if err != nil || filepath.Dir(relative) != "." || !strings.HasPrefix(relative, "vxs-fuzz-") {
		return fmt.Errorf("refusing to remove fuzz work outside the expected temporary root: %q", work)
	}
	if err := os.RemoveAll(target); err != nil {
		return fmt.Errorf("could not remove successful fuzz work %q: %w", work, err)
	}
	return nil
}
