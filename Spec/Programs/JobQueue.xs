// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Demonstrates a small async job queue with Result and Optional values.



import process, sync, stdio;

data Job {
    id: Int;
    command: String;
    retries: Int;
}

class Queue {
    jobs: ArrayList<Job>;

    Queue() {
        self.jobs = [];
    }

    fn push(job: Job) {
        self.jobs.append(job);
    }

    fn pop() -> Optional<Job> {
        if (self.jobs.count == 0) {
            return None;
        }

        return Some(self.jobs.remove(0));
    }
}

class Worker {
    static async fn run(queue: &mut Queue) -> Result<Int, Error> {
        completed: Int = 0;

        loop {
            maybe_job: Optional<Job> = queue.pop();
            if (maybe_job == None) {
                return Ok(completed);
            }

            job: Job = maybe_job!;
            result: Result<()> = await Worker::execute(job);
            result@;
            completed += 1;
        }
    }

    static async fn execute(job: Job) -> Result<()> {
        status: Int = await std::process::run(&job.command);
        if (status != 0) {
            return Error(new Error("job failed"));
        }

        return Ok();
    }
}

fn main() -> Result<Int, Error> {
    queue: Queue = new Queue();
    queue.push(Job { id: 1, command: new String("compile"), retries: 0 });
    queue.push(Job { id: 2, command: new String("test"), retries: 0 });

    completed: Int = await Worker::run(&mut queue)@;
    println!("completed {} jobs", completed);
    return Ok(0);
}
