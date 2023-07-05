#include "worker.h"
#include "backend/pcmanager/pclist.h"
#include "backend/pcmanager/priv.h"
#include "app.h"
#include "errors.h"

int worker_quit_app(worker_context_t *context) {
    GS_CLIENT client = app_gs_client_new(context->app);
    pclist_t *node = pclist_find_by_uuid(context->manager, &context->uuid);
    if (node == NULL) {
        return GS_ERROR;
    }
    int ret = gs_quit_app(client, node->server);
    pclist_unlock(context->manager);
    if (ret == GS_OK) {
        SERVER_STATE state = {.code = SERVER_STATE_AVAILABLE};
        pclist_upsert(context->manager, &context->uuid, &state, node->server);
    } else {
        const char *gs_error = NULL;
        gs_get_error(&gs_error);
        context->error = gs_error != NULL ? strdup(gs_error) : NULL;
    }
    return ret;
}

