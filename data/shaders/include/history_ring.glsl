#ifndef HISTORY_RING_GLSL
#define HISTORY_RING_GLSL

/// @brief Layer of a ring that holds the frame `age` frames old; age 0 is the newest.
/// @param head  The layer the next frame is written to, as sent after the weights in the default push block.
/// @param count Layers in the ring.
uint ring_layer(uint age, uint head, uint count)
{
    return (head + count - 1u - (age % count)) % count;
}

/// @brief Age of the frame held by a layer of a ring; 0 is the newest.
uint ring_age(uint layer, uint head, uint count)
{
    return (head + count - 1u - (layer % count)) % count;
}

#endif
