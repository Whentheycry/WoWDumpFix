#include "plugin.h"

#include "anti_debug_util.h"
#include "fix_dump.h"

Debuggee debuggee;

// Plugin exported command.
static const char cmdWoWDumpFix[] = "WoWDumpFix";

static const char authorName[] = "changeofpace";
static const char githubSourceURL[] = R"(https://github.com/changeofpace/WoW-Dump-Fix)";

///////////////////////////////////////////////////////////////////////////////
// Added Commands

static bool cbWoWDumpFix(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    pluginLog("Executing %s v%d.\n", PLUGIN_NAME, PLUGIN_VERSION);
    if (!fixdump::current::FixWoW()) {
        pluginLog("Failed to complete. Open an issue on github with the error message and log output:\n");
        pluginLog("    %s\n", githubSourceURL);
        return false;
    }
    pluginLog("Completed successfully. Use Scylla to dump Wow.exe.\n");
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// x64dbg

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* Info)
{
    ULONG cbImageSize = 0;
    BOOL status = TRUE;

    UNREFERENCED_PARAMETER(cbType);

    //
    // Zero global variables.
    //
    RtlSecureZeroMemory(&debuggee, sizeof(debuggee));

    INF_PRINT("===========================================================\n");
    INF_PRINT("                  Create Process Callback\n");
    INF_PRINT("===========================================================\n");

    //
    // Validate the callback parameters because we are compiling an old version
    //  of the plugin SDK.
    //
    if (!Info->fdProcessInfo->hProcess)
    {
        ERR_PRINT("Error: hProcess was null.\n");
        goto exit;
    }

    if (!Info->modInfo->BaseOfImage)
    {
        ERR_PRINT("Error: BaseOfImage was null.\n");
        goto exit;
    }

    //
    // NOTE Info.modInfo.ImageSize is always zero.
    //
    if (!fixdump::current::GetWoWImageSize(
            Info->fdProcessInfo->hProcess,
            &cbImageSize))
    {
        ERR_PRINT("Error: GetWoWImageSize failed.\n");
        goto exit;
    }

    //
    // Initialize global variables.
    //
    debuggee.hProcess = Info->fdProcessInfo->hProcess;
    debuggee.imageBase = Info->modInfo->BaseOfImage;
    debuggee.imageSize = cbImageSize;

    INF_PRINT("    hProcess:  0x%IX\n", debuggee.hProcess);
    INF_PRINT("    ImageBase: %p\n", debuggee.imageBase);
    INF_PRINT("    ImageSize: 0x%X\n", debuggee.imageSize);

    //
    // Remove anti-debugging techniques.
    //
    status = AduRevertPatchNtdllDbgBreakPoint(debuggee.hProcess);
    if (!status)
    {
        ERR_PRINT(
            "Failed to patch ntdll!DbgBreakPoint, debugger may fail to attach.\n");
        goto exit;
    }

exit:
    INF_PRINT("-----------------------------------------------------------\n");

    return;
}

PLUG_EXPORT void CBEXITPROCESS(CBTYPE cbType, EXIT_PROCESS_DEBUG_INFO* Info)
{
    UNREFERENCED_PARAMETER(cbType);
    UNREFERENCED_PARAMETER(Info);

    debuggee = {};
}

enum { PLUGIN_MENU_ABOUT };

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    UNREFERENCED_PARAMETER(cbType);

    switch (info->hEntry)
    {
    case PLUGIN_MENU_ABOUT: {
        const int maxMessageBoxStringSize = 1024;
        char buf[maxMessageBoxStringSize] = "";

        _snprintf_s(buf, maxMessageBoxStringSize, _TRUNCATE,
                    "Author:  %s\n\nSource Code:  %s",
                    authorName, githubSourceURL);

        MessageBoxA(hwndDlg, buf, "About", 0);
    }
    break;
    }
}

bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
    UNREFERENCED_PARAMETER(initStruct);

    if (!_plugin_registercommand(pluginHandle, cmdWoWDumpFix, cbWoWDumpFix, true)) {
        pluginLog("failed to register command %s.\n", cmdWoWDumpFix);
        return false;
    }
    return true;
}

bool pluginStop()
{
    _plugin_menuclear(hMenu);
    _plugin_unregistercommand(pluginHandle, cmdWoWDumpFix);
    return true;
}

void pluginSetup()
{
    _plugin_menuaddentry(hMenu, PLUGIN_MENU_ABOUT, "&About");
}

void pluginLog(const char* Format, ...)
{
    va_list valist;
    char buf[MAX_STRING_SIZE];
    RtlZeroMemory(buf, MAX_STRING_SIZE);

    _snprintf_s(buf, MAX_STRING_SIZE, _TRUNCATE, "[%s] ", PLUGIN_NAME);

    va_start(valist, Format);
    _vsnprintf_s(buf + strlen(buf), sizeof(buf) - strlen(buf), _TRUNCATE, Format, valist);
    va_end(valist);

    _plugin_logputs(buf);
}