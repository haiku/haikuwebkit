include(inspector/remote/Socket.cmake)

add_definitions(-D_DEFAULT_SOURCE)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSStringRefBSTR.h
    API/JavaScriptCore.h
    API/JSContext.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSValue.h
    API/JSVirtualMachine.h
)

install(FILES API/JavaScript.h
              API/JSBase.h
              API/JSContextRef.h
              API/JSObjectRef.h
              API/JSStringRef.h
              API/JSValueRef.h
              API/WebKitAvailability.h
        DESTINATION "${HEADER_INSTALL_DIR}/JavaScriptCore"
)

list(APPEND JavaScriptCore_LIBRARIES
	bsd network
)
