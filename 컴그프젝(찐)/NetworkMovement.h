#pragma once

bool StartNetworkMovement();

void StopNetworkMovement();

void NetworkSpecialKeyboard(
    int key,
    int x,
    int y
);

void NetworkKeyboard(
    unsigned char key,
    int x,
    int y
);

void NetworkTimer(int value);