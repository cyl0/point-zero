#ifndef POINT_ZERO_CORE_API_H
#define POINT_ZERO_CORE_API_H

#include <stdint.h>

#define PZ_CORE_ABI 5u
#define PZ_CALL __cdecl

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PZ_Bytes {
    const uint8_t* data;
    uint64_t size;
} PZ_Bytes;

/* Optional caller-owned buffer. Every call clears it, and truncates with NUL. */
typedef struct PZ_Diagnostic {
    char* data;
    uint32_t capacity;
} PZ_Diagnostic;

enum PZ_Status {
    PZ_OK = 0,
    PZ_INVALID_ARGUMENT = 1,
    PZ_PREPARATION_FAILED = 2,
    PZ_CANCELLED = 3,
    PZ_NATIVE_FAILED = 4
};

/* Paths are absolute UTF-8. Input buffers remain valid until prepare returns.
   The host captures each logical path once, even when backed by staged bytes.
   Callbacks contain all exceptions and may return PZ_CANCELLED on early stop. */
typedef int32_t (PZ_CALL *PZ_ReadInput)(void*, PZ_Bytes path, PZ_Bytes* out,
                                     PZ_Diagnostic diagnostic);
typedef struct PZ_PrepareRequest {
    uint32_t size;
    PZ_Bytes generation_id;
    PZ_Bytes config_path;
    void* input_context;
    PZ_ReadInput read_input;
    PZ_Bytes optional_cache;
} PZ_PrepareRequest;

typedef struct PZ_Prepared PZ_Prepared;

/* Terminal native admission, not preparation success. Exactly once after an
   accepted initialize call, including failure/shutdown before commitment.
   The context and function must remain alive until shutdown returns. */
typedef void (PZ_CALL *PZ_Completion)(void*, int32_t status);

/* Immutable SDK snapshot, not a supported-build decision. Model/datecode must
   be nonempty and NUL-terminated within these arrays. The core owns support
   policy; dest/spec/rev are the SDK's single-byte values (possibly zero). */
typedef struct PZ_AvsInfo {
    char model[4];
    char datecode[11];
    char dest, spec, rev;
} PZ_AvsInfo;

enum PZ_LogLevel {
    PZ_LOG_INFO = 0,
    PZ_LOG_WARNING = 1,
    PZ_LOG_ERROR = 2
};

/* UTF-8 message bytes are borrowed only for the duration of the callback.
   Callbacks contain all exceptions and may be called from native threads.
   Logging never requests process termination, including for PZ_LOG_ERROR.
   Notice is optional, best-effort, and conveys no delivery/admission guarantee.
   The native adapter chooses when a verified-ready notice is appropriate. */
typedef void (PZ_CALL *PZ_HostLog)(void*, int32_t level, PZ_Bytes message);
typedef void (PZ_CALL *PZ_NativeNotice)(void*, PZ_Bytes message);
/* Optional one-shot graphics-ready signal from an admitted, completed native
   scene boundary. Not an admission ACK or proof of visible overlay delivery.
   Lets the fixed loader deliver a pending startup outcome without game hooks
   of its own. Callback storage remains valid through shutdown. */
typedef void (PZ_CALL *PZ_NativeReady)(void*);
typedef struct PZ_NativeHost {
    PZ_AvsInfo avs;
    void* context;
    PZ_HostLog log;             /* Required. */
    PZ_NativeNotice notice;     /* NULL when unavailable. */
    PZ_NativeReady ready;       /* NULL when unavailable. */
} PZ_NativeHost;

typedef struct PZ_InitializeRequest {
    uint32_t size;
    PZ_Bytes runtime_path;
    PZ_Bytes generation_path;
    void* completion_context;
    PZ_Completion completion;
    PZ_NativeHost host;
} PZ_InitializeRequest;

/* Calls are serialized by the fixed loader; native admission itself is later.
   Attach/get_api/prepare/release install no hooks, workers or SDK callbacks and
   perform no filesystem/network I/O. The core copies every retained host byte.
   A module stays loaded while it owns objects or can execute callbacks. */
typedef struct PZ_CoreApi {
    uint32_t abi;
    uint32_t size;
    int32_t (PZ_CALL *prepare)(const PZ_PrepareRequest*, PZ_Prepared** out,
                             PZ_Diagnostic);
    /* Optional cache is core-owned, valid until release or initialize. Copy it
       before initialize; only publish the copy after successful native ACK. */
    PZ_Bytes (PZ_CALL *cache_bytes)(const PZ_Prepared*);
    void (PZ_CALL *release_prepared)(PZ_Prepared**);
    /* Invalid arguments retain ownership. Otherwise *owned becomes NULL before
       native initialization begins, even on failure. Never unload/retry a core
       after that transfer. Paths, metadata and callback pointers are copied
       before transfer; request storage may then be discarded. Host/completion
       contexts and callback code stay alive until shutdown returns; no callback
       may run afterward. No config reread, parsing or fitting here. */
    int32_t (PZ_CALL *initialize_prepared)(PZ_Prepared** owned,
                                        const PZ_InitializeRequest*, PZ_Diagnostic);
    void (PZ_CALL *shutdown)(void);
} PZ_CoreApi;

typedef const PZ_CoreApi* (PZ_CALL *PZ_GetCoreApi)(uint32_t requested_abi);

#ifdef __cplusplus
}
#endif
#endif
