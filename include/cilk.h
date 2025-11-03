#ifdef DEFERRED_SYNC_ENABLED
#define sync_current_stream() __kitcuda_sync_current_stream()
#define tapir_deferred_sync [[tapir::deferred_sync]]
#else
#define sync_current_stream()
#define tapir_deferred_sync
#endif

#define cilk_gpu_for [[tapir::target("cuda"), tapir::grain_size(1)]] cilk_for