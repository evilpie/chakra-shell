#include "stdafx.h"
#include <string>

#include <windows.h>
#include <jsrt.h>


// Modified by Tom Schuster
// Based on code by Paul Vick licensed unser Apache License, Version 2.0.
// http://code.msdn.microsoft.com/windowsdesktop/JavaScript-Runtime-Hosting-d3a13880



using namespace std;

//
// Source context counter.
//

unsigned currentSourceContext = 0;

//
// This "throws" an exception in the Chakra space. Useful routine for callbacks
// that need to throw a JS error to indicate failure.
//

void ThrowException(wstring errorString)
{
	// We ignore error since we're already in an error state.
	JsValueRef errorValue;
	JsValueRef errorObject;
	JsPointerToString(errorString.c_str(), errorString.length(), &errorValue);
	JsCreateError(errorValue, &errorObject);
	JsSetException(errorObject);
}



//
// Helper to load a script from disk.
//

wstring LoadScript(wstring fileName)
{
	FILE *file;
	if (_wfopen_s(&file, fileName.c_str(), L"rb"))
	{
		fwprintf(stderr, L"chakrahost: unable to open file: %s.\n", fileName.c_str());
		return wstring();
	}

	unsigned int current = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned int end = ftell(file);
	fseek(file, current, SEEK_SET);
	unsigned int lengthBytes = end - current;
	char *rawBytes = (char *) calloc(lengthBytes + 1, sizeof(char) );

	if (rawBytes == nullptr)
	{
		fwprintf(stderr, L"chakrahost: fatal error.\n");
		return wstring();
	}

	fread((void *) rawBytes, sizeof(char) , lengthBytes, file);

	wchar_t *contents = (wchar_t *) calloc(lengthBytes + 1, sizeof(wchar_t) );
	if (contents == nullptr)
	{
		free(rawBytes);
		fwprintf(stderr, L"chakrahost: fatal error.\n");
		return wstring();
	}

	if (MultiByteToWideChar(CP_UTF8, 0, rawBytes, lengthBytes + 1, contents, lengthBytes + 1) == 0)
	{
        free(rawBytes);
        free(contents);
		fwprintf(stderr, L"chakrahost: fatal error.\n");
		return wstring();
	}

	wstring result = contents;
    free(rawBytes);
    free(contents);
	return result;
}

//
// Callback to echo something to the command-line.
//

JsValueRef CALLBACK Print(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
	for (unsigned int index = 1; index < argumentCount; index++)
	{
		if (index > 1)
		{
			wprintf(L" ");
		}

		JsValueRef stringValue;
		IfFailThrow(JsConvertValueToString(arguments[index], &stringValue), L"invalid argument");

		const wchar_t *string;
		size_t length;
		IfFailThrow(JsStringToPointer(stringValue, &string, &length), L"invalid argument");

		wprintf(L"%s", string);
	}

	wprintf(L"\n");

	return JS_INVALID_REFERENCE;
}

//
// Callback to load a script and run it.
//

JsValueRef CALLBACK Load(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
	JsValueRef result = JS_INVALID_REFERENCE;

	if (argumentCount < 2)
	{
		ThrowException(L"not enough arguments");
		return result;
	}

	//
	// Convert filename.
	//
	const wchar_t *filename;
	size_t length;

	IfFailThrow(JsStringToPointer(arguments[1], &filename, &length), L"invalid filename argument");

	//
	// Load the script from the disk.
	//

	wstring script = LoadScript(filename);
	if (script.empty())
	{
		ThrowException(L"invalid script");
		return result;
	}

	//
	// Run the script.
	//

	IfFailThrow(JsRunScript(script.c_str(), currentSourceContext++, filename, &result), L"failed to run script.");

	return result;
}

JsValueRef CALLBACK Read(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
	JsValueRef result = JS_INVALID_REFERENCE;

	if (argumentCount < 2)
	{
		ThrowException(L"not enough arguments");
		return result;
	}

	//
	// Convert filename.
	//
	const wchar_t *filename;
	size_t length;

	IfFailThrow(JsStringToPointer(arguments[1], &filename, &length), L"invalid filename argument");

	//
	// Load file from disk.
	//

	wstring script = LoadScript(filename);
	if (script.empty())
	{
		ThrowException(L"invalid file");
		return result;
	}

	//
	// Return read data.
	//

	IfFailThrow(JsPointerToString(script.c_str(), script.length(), &result), L"failed to convert string.");

	return result;
}

//
// Helper to define a host callback method on the global host object.
//

JsErrorCode DefineHostCallback(JsValueRef globalObject, const wchar_t *callbackName, JsNativeFunction callback, void *callbackState)
{
	//
	// Get property ID.
	//

	JsPropertyIdRef propertyId;
	IfFailRet(JsGetPropertyIdFromName(callbackName, &propertyId));

	//
	// Create a function
	//

	JsValueRef function;
	IfFailRet(JsCreateFunction(callback, callbackState, &function));

	//
	// Set the property
	//

	IfFailRet(JsSetProperty(globalObject, propertyId, function, true));

	return JsNoError;
}

//
// Creates a host execution context and sets up the host object in it.
//

JsErrorCode CreateHostContext(JsRuntimeHandle runtime, JsContextRef *context)
{
	//
	// Create the context. Note that if we had wanted to start debugging from the very
	// beginning, we would have passed in an IDebugApplication pointer here.
	//

	IfFailRet(JsCreateContext(runtime, nullptr, context));

	//
	// Now set the execution context as being the current one on this thread.
	//

	IfFailRet(JsSetCurrentContext(*context));


	//
	// Get the global object
	//

	JsValueRef globalObject;
	IfFailRet(JsGetGlobalObject(&globalObject));


	//
	// Now create the host callbacks that we're going to expose to the script.
	//

	IfFailRet(DefineHostCallback(globalObject, L"print", Print, nullptr));
    IfFailRet(DefineHostCallback(globalObject, L"load", Load, nullptr));
	IfFailRet(DefineHostCallback(globalObject, L"read", Read, nullptr));


	//
	// Clean up the current execution context.
	//

	IfFailRet(JsSetCurrentContext(JS_INVALID_REFERENCE));

	return JsNoError;
}

//
// Print out a script exception.
//

JsErrorCode PrintScriptException()
{
	fwprintf(stderr, L"Caught exception!\n");

	//
	// Get script exception.
	//

	JsValueRef exception;
	IfFailRet(JsGetAndClearException(&exception));

	//
	// Get message.
	//

	JsPropertyIdRef messageName;
	IfFailRet(JsGetPropertyIdFromName(L"message", &messageName));

	JsValueRef messageValue;
	IfFailRet(JsGetProperty(exception, messageName, &messageValue));

	const wchar_t *message;
	size_t length;
	IfFailRet(JsStringToPointer(messageValue, &message, &length));

	//
	// Get stack.
	//

	JsPropertyIdRef stackName;
	IfFailRet(JsGetPropertyIdFromName(L"stack", &stackName));

	JsValueRef stackValue;
	IfFailRet(JsGetProperty(exception, stackName, &stackValue));

	const wchar_t *stack;
	IfFailRet(JsStringToPointer(stackValue, &stack, &length));

	fwprintf(stderr, L"chakrahost: exception: %s\n", message);
	fwprintf(stderr, L"stack:\n%s\n", stack);

	return JsNoError;
}

//
// The main entry point for the host.
//

int _cdecl wmain(int argc, wchar_t *argv[])
{
	int returnValue = EXIT_FAILURE;

	if (argc < 1)
	{
		fwprintf(stderr, L"usage: chakrahost <script name>\n");
		return returnValue;
	}

	try
	{
		JsRuntimeHandle runtime;
		JsContextRef context;

		//
		// Create the runtime. We're only going to use one runtime for this host.
		//

		IfFailError(JsCreateRuntime(JsRuntimeAttributeNone, JsRuntimeVersion11, nullptr, &runtime), L"failed to create runtime.");

		//
		// Similarly, create a single execution context. Note that we're putting it on the stack here,
		// so it will stay alive through the entire run.
		//

		IfFailError(CreateHostContext(runtime, &context), L"failed to create execution context.");

		//
		// Now set the execution context as being the current one on this thread.
		//

		IfFailError(JsSetCurrentContext(context), L"failed to set current context.");

		//
		// Load the script from the disk.
		//

		wstring script = LoadScript(argv[1]);
		if (script.empty())
		{
			goto error;
		}

		//
		// Run the script.
		//

		JsValueRef result;
		JsErrorCode errorCode = JsRunScript(script.c_str(), currentSourceContext++, argv[1], &result);
		
		if (errorCode == JsErrorScriptException)
		{
			IfFailError(PrintScriptException(), L"failed to print exception");
			return EXIT_FAILURE;
		}
		else
		{
			IfFailError(errorCode, L"failed to run script.");
		}

		//
		// Convert the return value.
		//

		JsValueRef numberResult;
		double doubleResult;
		IfFailError(JsConvertValueToNumber(result, &numberResult), L"failed to convert return value.");
		IfFailError(JsNumberToDouble(numberResult, &doubleResult), L"failed to convert return value.");
		returnValue = (int) doubleResult;

		//
		// Clean up the current execution context.
		//

		IfFailError(JsSetCurrentContext(JS_INVALID_REFERENCE), L"failed to cleanup current context.");

		//
		// Clean up the runtime.
		//

		IfFailError(JsDisposeRuntime(runtime), L"failed to cleanup runtime.");
	}
	catch (...)
	{
		fwprintf(stderr, L"chakrahost: fatal error: internal error.\n");
	}

error:
	return returnValue;
}
