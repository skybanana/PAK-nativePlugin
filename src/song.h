#pragma once

struct PluginState;

// Initializes the MP3 decoder used by the native song player.
bool initializeSongDecoder(void);

// Decodes the loaded chart's song into the plugin PCM buffer.
bool loadSong(PluginState *state, const char *chartPath);

// Releases the MP3 decoder used by the native song player.
void shutdownSongDecoder(void);
