#include "raxel_window.h"

#include <SFML/Audio.h>
#include <SFML/Graphics.h>
#include <SFML/Network.h>
#include <SFML/System.h>

void raxel_window_test()
{
        // Create a window
    sfVideoMode mode = {800, 600, 32};
    sfRenderWindow* window =
        sfRenderWindow_create(mode, "CSFML Test", sfResize | sfClose, NULL);
    if (!window) return -1;

    // Create a circle shape (Graphics)
    sfCircleShape* circle = sfCircleShape_create();
    sfCircleShape_setRadius(circle, 50.0f);
    sfCircleShape_setFillColor(circle, sfBlue);
    sfCircleShape_setPosition(circle, (sfVector2f){300.0f, 200.0f});

    // Create a clock (System)
    sfClock* clock = sfClock_create();

    // Create a UDP socket (Network)
    sfUdpSocket* socket = sfUdpSocket_create();
    if (socket) {
        printf("Network module initialized!\n");
    }

    // Create a sound buffer and sound (Audio)
    sfSoundBuffer* soundBuffer = sfSoundBuffer_createFromFile("../sound.mp3");
    sfSound* sound = NULL;
    if (soundBuffer) {
        sound = sfSound_create();
        sfSound_setBuffer(sound, soundBuffer);
        printf("Audio module initialized!\n");
    }

    // Main loop
    while (sfRenderWindow_isOpen(window)) {
        sfEvent event;
        while (sfRenderWindow_pollEvent(window, &event)) {
            if (event.type == sfEvtClosed) sfRenderWindow_close(window);

            // Play sound on mouse click
            if (event.type == sfEvtMouseButtonPressed && sound) {
                sfSound_play(sound);
            }
        }

        // Update circle position using time
        sfTime elapsed = sfClock_getElapsedTime(clock);
        float seconds = sfTime_asSeconds(elapsed);
        sfCircleShape_setPosition(
            circle, (sfVector2f){300.0f + seconds * 100.0f, 200.0f});

        sfRenderWindow_clear(window, sfBlack);
        sfRenderWindow_drawCircleShape(window, circle, NULL);
        sfRenderWindow_display(window);
    }

    // Cleanup
    sfClock_destroy(clock);
    sfCircleShape_destroy(circle);
    if (sound) {
        sfSound_destroy(sound);
        sfSoundBuffer_destroy(soundBuffer);
    }
    if (socket) {
        sfUdpSocket_destroy(socket);
    }
    sfRenderWindow_destroy(window);

    return 0;
}