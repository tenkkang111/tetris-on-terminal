#include <stdlib.h>
#include "scene.h"
#include "ui.h"

int main(void)
{
    uiInit();

    SceneContext ctx;
    sceneContextInit(&ctx);

    SceneType currentScene = SCENE_LOBBY;

    while (currentScene != SCENE_QUIT) {
        switch (currentScene) {
            case SCENE_LOBBY:
                currentScene = sceneLobby(&ctx);
                break;
            case SCENE_MATCHING:
                currentScene = sceneMatching(&ctx);
                break;
            case SCENE_GAME:
                currentScene = sceneGame(&ctx);
                break;
            case SCENE_RESULT:
                currentScene = sceneResult(&ctx);
                break;
            case SCENE_QUIT:
                break;
        }
    }

    uiShutdown();
    return 0;
}
