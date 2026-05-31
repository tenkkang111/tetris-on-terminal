#include "scene.h"
#include <string.h>

void sceneContextInit(SceneContext* ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->netMode = NET_MODE_NONE;
    ctx->port = 5555;
    ctx->net.sock = -1;
}
