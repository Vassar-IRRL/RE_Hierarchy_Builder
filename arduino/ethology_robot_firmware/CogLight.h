#ifndef COGLIGHT_H
#define COGLIGHT_H

#include <Arduino.h>
#include "CogAnaDigi.h"

// Analog light sensor derived from CogAnaDigi.
class CogLight : public CogAnaDigi {
public:
    // Require a labeled analog pin, e.g., "A0", "A3"
    explicit CogLight(const String& pinLabel);

    // Read raw sensor value via base class, mirror to subclass _rawData
    int getRawData();

    // Map raw [0..1023] to [0..100] and return
    int getData();
    // Return the LAST value computed by getData() WITHOUT taking a new
    // sample. Use this when you need the exact reading a prior getData()
    // used (e.g. logging the same sample the control math consumed).
    int peekData() const { return _data; }

private:
    int _pin;      // mirrored pin reference
    int _rawData;  // last raw reading
    int _data;     // processed/normalized data
};

#endif // COGLIGHT_H