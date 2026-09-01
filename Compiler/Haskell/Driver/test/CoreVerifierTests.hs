-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module CoreVerifierTests (coreVerifierTests) where

import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Diagnostic

-- The optimizer is guarded by the same verifier before and after rewriting.
-- These focused malformed-tree cases make that boundary explicit and ensure
-- diagnostics continue to identify the invariant that was violated.
coreVerifierTests :: [(String, Bool)]
coreVerifierTests =
    [ ("Core verifier accepts the minimal unit module", accepted minimalUnitModule)
    , ("Core verifier rejects an empty module name", rejectedWith "VXC1001" emptyModuleName)
    , ("Core verifier rejects an empty module-name segment", rejectedWith "VXC1001" emptyModuleSegment)
    , ("Core verifier rejects duplicate function symbols", rejectedWith "VXC1002" duplicateFunctions)
    , ("Core verifier rejects an unresolved function result", rejectedWith "VXC1003" unresolvedFunctionResult)
    , ("Core verifier rejects duplicate parameter symbols", rejectedWith "VXC1004" duplicateParameters)
    , ("Core verifier rejects a missing value return", rejectedWith "VXC1005" missingValueReturn)
    , ("Core verifier rejects a zero function symbol", rejectedWith "VXC1006" zeroFunctionSymbol)
    , ("Core verifier rejects a negative parameter symbol", rejectedWith "VXC1006" negativeParameterSymbol)
    , ("Core verifier rejects an unresolved parameter type", rejectedWith "VXC1007" unresolvedParameterType)
    , ("Core verifier rejects a zero binding symbol", rejectedWith "VXC1008" zeroBindingSymbol)
    , ("Core verifier rejects an unresolved binding type", rejectedWith "VXC1009" unresolvedBindingType)
    , ("Core verifier rejects a redefined local symbol", rejectedWith "VXC1010" duplicateBinding)
    , ("Core verifier rejects a binding value type mismatch", rejectedWith "VXC1011" bindingTypeMismatch)
    , ("Core verifier rejects assignment to an undefined symbol", rejectedWith "VXC1012" undefinedAssignment)
    , ("Core verifier rejects assignment to immutable storage", rejectedWith "VXC1013" immutableAssignment)
    , ("Core verifier rejects assignment with the wrong value type", rejectedWith "VXC1014" assignmentTypeMismatch)
    , ("Core verifier rejects a zero assignment symbol", rejectedWith "VXC1015" zeroAssignmentSymbol)
    , ("Core verifier rejects a return value type mismatch", rejectedWith "VXC1016" returnTypeMismatch)
    , ("Core verifier rejects a String condition", rejectedWith "VXC1017" stringCondition)
    , ("Core verifier rejects an unresolved expression type", rejectedWith "VXC1018" unresolvedExpression)
    , ("Core verifier rejects a zero variable symbol", rejectedWith "VXC1019" zeroVariableSymbol)
    , ("Core verifier rejects an undefined variable", rejectedWith "VXC1020" undefinedVariable)
    , ("Core verifier rejects variable type disagreement", rejectedWith "VXC1021" variableTypeMismatch)
    , ("Core verifier rejects a call with too few arguments", rejectedWith "VXC1022" callArityMismatch)
    , ("Core verifier rejects a call argument type mismatch", rejectedWith "VXC1023" callArgumentMismatch)
    , ("Core verifier rejects a call result type mismatch", rejectedWith "VXC1024" callResultMismatch)
    , ("Core verifier rejects a non-callable target", rejectedWith "VXC1025" nonCallableTarget)
    , ("Core verifier rejects a primitive arity mismatch", rejectedWith "VXC1026" primitiveArityMismatch)
    , ("Core verifier rejects String arithmetic", rejectedWith "VXC1027" stringArithmetic)
    , ("Core verifier rejects mixed numeric operand types", rejectedWith "VXC1027" mixedArithmetic)
    , ("Core verifier rejects a primitive result mismatch", rejectedWith "VXC1028" primitiveResultMismatch)
    , ("Core verifier rejects an overflowing byte literal", rejectedWith "VXC1029" overflowingByteLiteral)
    , ("Core verifier rejects a floating payload on int", rejectedWith "VXC1029" floatingPayloadOnInt)
    , ("Core verifier rejects malformed floating spelling", rejectedWith "VXC1029" malformedFloatingSpelling)
    , ("Core verifier rejects duplicate closure captures", rejectedWith "VXC1030" duplicateCaptures)
    , ("Core verifier rejects duplicate closure parameters", rejectedWith "VXC1031" duplicateClosureParameters)
    , ("Core verifier rejects a closure missing a return", rejectedWith "VXC1032" closureMissingReturn)
    , ("Core verifier rejects closure parameter disagreement", rejectedWith "VXC1033" closureParameterMismatch)
    , ("Core verifier rejects closure callable arity mismatch", rejectedWith "VXC1034" closureArityMismatch)
    , ("Core verifier rejects closure result disagreement", rejectedWith "VXC1035" closureResultMismatch)
    , ("Core verifier rejects a non-callable closure type", rejectedWith "VXC1036" closureNotCallable)
    , ("Core verifier rejects a zero capture symbol", rejectedWith "VXC1037" zeroCaptureSymbol)
    , ("Core verifier rejects an unresolved capture type", rejectedWith "VXC1038" unresolvedCaptureType)
    , ("Core verifier rejects a capture initializer mismatch", rejectedWith "VXC1039" captureInitializerMismatch)
    , ("Core verifier accepts numeric conditions", accepted numericCondition)
    , ("Core verifier accepts boolean conditions", accepted booleanCondition)
    , ("Core verifier accepts all-returning branches", accepted allReturningBranch)
    , ("Core verifier accepts a well-typed direct call", accepted validDirectCall)
    , ("Core verifier accepts a well-typed closure", accepted validClosure)
    ]

resolved :: Int -> String -> ResolvedName
resolved symbol spelling = ResolvedName (SymbolId symbol) (Identifier spelling)

moduleName :: QualifiedName
moduleName = QualifiedName [Identifier "Verifier", Identifier "Tests"]

mainName, helperName, valueName, captureValueName :: ResolvedName
mainName = resolved 1 "Main"
helperName = resolved 2 "Helper"
valueName = resolved 10 "value"
captureValueName = resolved 12 "captured"

unitValue, intValue, boolValue, stringValue :: CoreExpression
unitValue = CoreLiteral CoreUnit unitType
intValue = CoreLiteral (CoreInteger 1) intType
boolValue = CoreLiteral (CoreBoolean True) boolType
stringValue = CoreLiteral (CoreString "value") stringType

function :: ResolvedName -> [(ResolvedName, Type)] -> Type -> [CoreStatement] -> CoreFunction
function = CoreFunction

unitFunction :: ResolvedName -> [CoreStatement] -> CoreFunction
unitFunction functionName body = function functionName [] unitType (body ++ [CoreReturn unitValue])

coreModule :: [CoreFunction] -> CoreModule
coreModule = CoreModule moduleName

accepted :: CoreModule -> Bool
accepted moduleValue = verifyCore moduleValue == Right moduleValue

rejectedWith :: String -> CoreModule -> Bool
rejectedWith code moduleValue = case verifyCore moduleValue of
    Left diagnostics -> any ((== code) . diagnosticCode) diagnostics
    Right _ -> False

minimalUnitModule :: CoreModule
minimalUnitModule = coreModule [unitFunction mainName []]

emptyModuleName :: CoreModule
emptyModuleName = CoreModule (QualifiedName []) [unitFunction mainName []]

emptyModuleSegment :: CoreModule
emptyModuleSegment = CoreModule (QualifiedName [Identifier "Verifier", Identifier ""]) [unitFunction mainName []]

duplicateFunctions :: CoreModule
duplicateFunctions = coreModule [unitFunction mainName [], unitFunction mainName []]

unresolvedFunctionResult :: CoreModule
unresolvedFunctionResult = coreModule [function mainName [] ErrorType [CoreReturn unitValue]]

duplicateParameters :: CoreModule
duplicateParameters =
    coreModule [function mainName [(valueName, intType), (valueName, intType)] unitType [CoreReturn unitValue]]

missingValueReturn :: CoreModule
missingValueReturn = coreModule [function mainName [] intType []]

zeroFunctionSymbol :: CoreModule
zeroFunctionSymbol = coreModule [unitFunction (resolved 0 "Main") []]

negativeParameterSymbol :: CoreModule
negativeParameterSymbol =
    coreModule [function mainName [(resolved (-1) "value", intType)] unitType [CoreReturn unitValue]]

unresolvedParameterType :: CoreModule
unresolvedParameterType = coreModule [function mainName [(valueName, ErrorType)] unitType [CoreReturn unitValue]]

zeroBindingSymbol :: CoreModule
zeroBindingSymbol =
    coreModule [unitFunction mainName [CoreBind (CoreBinding (resolved 0 "value") intType False intValue)]]

unresolvedBindingType :: CoreModule
unresolvedBindingType =
    coreModule [unitFunction mainName [CoreBind (CoreBinding valueName ErrorType False intValue)]]

duplicateBinding :: CoreModule
duplicateBinding =
    coreModule
        [ unitFunction
            mainName
            [ CoreBind (CoreBinding valueName intType False intValue)
            , CoreBind (CoreBinding valueName intType False intValue)
            ]
        ]

bindingTypeMismatch :: CoreModule
bindingTypeMismatch =
    coreModule [unitFunction mainName [CoreBind (CoreBinding valueName boolType False intValue)]]

undefinedAssignment :: CoreModule
undefinedAssignment = coreModule [unitFunction mainName [CoreAssign valueName intValue]]

immutableAssignment :: CoreModule
immutableAssignment =
    coreModule
        [ unitFunction
            mainName
            [ CoreBind (CoreBinding valueName intType False intValue)
            , CoreAssign valueName intValue
            ]
        ]

assignmentTypeMismatch :: CoreModule
assignmentTypeMismatch =
    coreModule
        [ unitFunction
            mainName
            [ CoreBind (CoreBinding valueName intType True intValue)
            , CoreAssign valueName boolValue
            ]
        ]

zeroAssignmentSymbol :: CoreModule
zeroAssignmentSymbol = coreModule [unitFunction mainName [CoreAssign (resolved 0 "value") intValue]]

returnTypeMismatch :: CoreModule
returnTypeMismatch = coreModule [function mainName [] boolType [CoreReturn intValue]]

stringCondition :: CoreModule
stringCondition = coreModule [unitFunction mainName [CoreIf stringValue [] []]]

unresolvedExpression :: CoreModule
unresolvedExpression =
    coreModule [unitFunction mainName [CoreEvaluate (CoreLiteral (CoreInteger 1) ErrorType)]]

zeroVariableSymbol :: CoreModule
zeroVariableSymbol =
    coreModule [unitFunction mainName [CoreEvaluate (CoreVariable (resolved 0 "value") intType)]]

undefinedVariable :: CoreModule
undefinedVariable = coreModule [unitFunction mainName [CoreEvaluate (CoreVariable valueName intType)]]

variableTypeMismatch :: CoreModule
variableTypeMismatch =
    coreModule
        [ function
            mainName
            [(valueName, intType)]
            unitType
            [CoreEvaluate (CoreVariable valueName boolType), CoreReturn unitValue]
        ]

helperFunction :: CoreFunction
helperFunction = function helperName [(valueName, intType)] intType [CoreReturn (CoreVariable valueName intType)]

helperReference :: CoreExpression
helperReference = CoreVariable helperName (FunctionType [intType] intType)

callArityMismatch :: CoreModule
callArityMismatch =
    coreModule [unitFunction mainName [CoreEvaluate (CoreApply helperReference [] intType)], helperFunction]

callArgumentMismatch :: CoreModule
callArgumentMismatch =
    coreModule [unitFunction mainName [CoreEvaluate (CoreApply helperReference [boolValue] intType)], helperFunction]

callResultMismatch :: CoreModule
callResultMismatch =
    coreModule [unitFunction mainName [CoreEvaluate (CoreApply helperReference [intValue] boolType)], helperFunction]

nonCallableTarget :: CoreModule
nonCallableTarget =
    coreModule
        [ function
            mainName
            [(valueName, intType)]
            unitType
            [CoreEvaluate (CoreApply (CoreVariable valueName intType) [] intType), CoreReturn unitValue]
        ]

primitiveArityMismatch :: CoreModule
primitiveArityMismatch =
    coreModule [unitFunction mainName [CoreEvaluate (CorePrimitive CoreAdd [intValue] intType)]]

stringArithmetic :: CoreModule
stringArithmetic =
    coreModule [unitFunction mainName [CoreEvaluate (CorePrimitive CoreAdd [stringValue, stringValue] stringType)]]

mixedArithmetic :: CoreModule
mixedArithmetic =
    let longValue = CoreLiteral (CoreInteger 1) (namedType "long")
     in coreModule [unitFunction mainName [CoreEvaluate (CorePrimitive CoreAdd [intValue, longValue] intType)]]

primitiveResultMismatch :: CoreModule
primitiveResultMismatch =
    coreModule [unitFunction mainName [CoreEvaluate (CorePrimitive CoreAdd [intValue, intValue] boolType)]]

overflowingByteLiteral :: CoreModule
overflowingByteLiteral =
    coreModule [unitFunction mainName [CoreEvaluate (CoreLiteral (CoreInteger 128) (namedType "byte"))]]

floatingPayloadOnInt :: CoreModule
floatingPayloadOnInt =
    coreModule [unitFunction mainName [CoreEvaluate (CoreLiteral (CoreFloating "1.0") intType)]]

malformedFloatingSpelling :: CoreModule
malformedFloatingSpelling =
    coreModule [unitFunction mainName [CoreEvaluate (CoreLiteral (CoreFloating "1e") (namedType "float"))]]

closure :: [CoreCapture] -> [(ResolvedName, Type)] -> Type -> [CoreStatement] -> Type -> CoreExpression
closure = CoreClosure

validClosureValue :: CoreExpression
validClosureValue =
    closure
        []
        [(valueName, intType)]
        intType
        [CoreReturn (CoreVariable valueName intType)]
        (FunctionType [intType] intType)

duplicateCaptures :: CoreModule
duplicateCaptures =
    let capture = CoreCapture StrongCapture captureValueName intType intValue
        value = closure [capture, capture] [] unitType [CoreReturn unitValue] (FunctionType [] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

duplicateClosureParameters :: CoreModule
duplicateClosureParameters =
    let value =
            closure
                []
                [(valueName, intType), (valueName, intType)]
                intType
                [CoreReturn intValue]
                (FunctionType [intType, intType] intType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

closureMissingReturn :: CoreModule
closureMissingReturn =
    let value = closure [] [] intType [] (FunctionType [] intType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

closureParameterMismatch :: CoreModule
closureParameterMismatch =
    let value = closure [] [(valueName, boolType)] unitType [CoreReturn unitValue] (FunctionType [intType] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

closureArityMismatch :: CoreModule
closureArityMismatch =
    let value = closure [] [(valueName, intType)] unitType [CoreReturn unitValue] (FunctionType [] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

closureResultMismatch :: CoreModule
closureResultMismatch =
    let value = closure [] [] unitType [CoreReturn unitValue] (FunctionType [] intType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

closureNotCallable :: CoreModule
closureNotCallable =
    let value = closure [] [] unitType [CoreReturn unitValue] intType
     in coreModule [unitFunction mainName [CoreEvaluate value]]

zeroCaptureSymbol :: CoreModule
zeroCaptureSymbol =
    let capture = CoreCapture StrongCapture (resolved 0 "captured") intType intValue
        value = closure [capture] [] unitType [CoreReturn unitValue] (FunctionType [] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

unresolvedCaptureType :: CoreModule
unresolvedCaptureType =
    let capture = CoreCapture StrongCapture captureValueName ErrorType intValue
        value = closure [capture] [] unitType [CoreReturn unitValue] (FunctionType [] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

captureInitializerMismatch :: CoreModule
captureInitializerMismatch =
    let capture = CoreCapture StrongCapture captureValueName boolType intValue
        value = closure [capture] [] unitType [CoreReturn unitValue] (FunctionType [] unitType)
     in coreModule [unitFunction mainName [CoreEvaluate value]]

numericCondition :: CoreModule
numericCondition = coreModule [unitFunction mainName [CoreIf intValue [] []]]

booleanCondition :: CoreModule
booleanCondition = coreModule [unitFunction mainName [CoreIf boolValue [] []]]

allReturningBranch :: CoreModule
allReturningBranch =
    coreModule
        [ function
            mainName
            [(valueName, boolType)]
            intType
            [ CoreIf
                (CoreVariable valueName boolType)
                [CoreReturn (CoreLiteral (CoreInteger 1) intType)]
                [CoreReturn (CoreLiteral (CoreInteger 2) intType)]
            ]
        ]

validDirectCall :: CoreModule
validDirectCall =
    coreModule
        [ function mainName [] intType [CoreReturn (CoreApply helperReference [intValue] intType)]
        , helperFunction
        ]

validClosure :: CoreModule
validClosure = coreModule [unitFunction mainName [CoreEvaluate validClosureValue]]
