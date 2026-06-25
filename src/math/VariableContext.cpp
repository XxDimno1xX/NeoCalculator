/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * VariableContext.cpp
 */

#include "VariableContext.h"
#include <string.h>

VariableContext::VariableContext() {
    memset(&_data, 0, sizeof(_data));
    _data.magic = MAGIC;
}

int VariableContext::indexFromName(char name) const {
    if (name >= 'A' && name <= 'Z') return name - 'A';
    if (name >= 'a' && name <= 'z') return name - 'a';
    return -1;
}



#ifdef ARDUINO
void VariableContext::loadFromNVS() {
    if (!_prefs.begin(NVS_NAMESPACE, true)) {
        // No se pudo abrir en solo lectura, inicializar en blanco
        memset(&_data, 0, sizeof(_data));
        _data.magic = MAGIC;
        _prefs.end();
        return;
    }

    size_t len = _prefs.getBytesLength(NVS_KEY);
    if (len == sizeof(PackedVars)) {
        PackedVars tmp;
        _prefs.getBytes(NVS_KEY, &tmp, sizeof(PackedVars));
        if (tmp.magic == MAGIC) {
            _data = tmp;
        } else {
            memset(&_data, 0, sizeof(_data));
            _data.magic = MAGIC;
        }
    } else {
        memset(&_data, 0, sizeof(_data));
        _data.magic = MAGIC;
    }
    _prefs.end();
}

void VariableContext::saveToNVS() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
        return;
    }
    _data.magic = MAGIC;
    _prefs.putBytes(NVS_KEY, &_data, sizeof(PackedVars));
    _prefs.end();
}
#else
// Native (emulator_pc): NVS/Preferences do not exist. VariableContext.h already
// provides inline no-op loadFromNVS()/saveToNVS() stubs for the non-Arduino path,
// so defining them here too would be a redefinition. Firmware keeps the real NVS
// implementations above unchanged.
#endif // ARDUINO

bool VariableContext::setVariable(char name, double value) {
    int idx = indexFromName(name);
    if (idx < 0 || idx >= 26) return false;
    _data.vars[idx] = value;
    _data.has[idx] = 1;
    // Ans no se persiste automáticamente aquí; solo A-Z.
    return true;
}

bool VariableContext::getVariable(char name, double &outValue) const {
    int idx = indexFromName(name);
    if (idx < 0 || idx >= 26) return false;
    if (_data.has[idx] == 0) return false;
    outValue = _data.vars[idx];
    return true;
}

void VariableContext::setAns(double value) {
    _data.ans = value;
}

double VariableContext::getAns() const {
    return _data.ans;
}

