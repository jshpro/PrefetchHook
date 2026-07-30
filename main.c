#include <ntifs.h>
#include <ntddk.h>
#include <fltKernel.h>

PFLT_FILTER g_FilterHandle = NULL;
PDRIVER_OBJECT g_DriverObject = NULL;

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

FLT_PREOP_CALLBACK_STATUS PrefetchWriteCheck(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

NTSTATUS InitPrefetchBlocker(VOID);
VOID CleanupPrefetchBlocker(VOID);

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_WRITE, 0, NULL, PrefetchWriteCheck },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    PAGED_CODE();
    g_DriverObject = DriverObject;
    DriverObject->DriverUnload = DriverUnload;
    return InitPrefetchBlocker();
}

VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    PAGED_CODE();
    CleanupPrefetchBlocker();
}

FLT_PREOP_CALLBACK_STATUS
PrefetchWriteCheck(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    NTSTATUS Status;
    PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;
    UNICODE_STRING PrefetchSig;
    BOOLEAN IsMatch = FALSE;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    PAGED_CODE();

    if (FlagOn(Data->Iopb->IrpFlags, IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    Status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &FileNameInfo
    );

    if (!NT_SUCCESS(Status) || (FileNameInfo == NULL)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    RtlInitUnicodeString(&PrefetchSig, L"\\Windows\\Prefetch\\");

    IsMatch = RtlFindUnicodeSubstring(
        &FileNameInfo->Name,
        &PrefetchSig,
        TRUE
    );

    FltReleaseFileNameInformation(FileNameInfo);

    if (IsMatch) {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;
        return FLT_PREOP_COMPLETE;
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

NTSTATUS
InitPrefetchBlocker(VOID)
{
    NTSTATUS Status;
    PAGED_CODE();

    Status = FltRegisterFilter(
        g_DriverObject,
        &FilterRegistration,
        &g_FilterHandle
    );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = FltStartFiltering(g_FilterHandle);
    if (!NT_SUCCESS(Status)) {
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
    }

    return Status;
}

VOID
CleanupPrefetchBlocker(VOID)
{
    PAGED_CODE();
    if (g_FilterHandle != NULL) {
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
    }
}
