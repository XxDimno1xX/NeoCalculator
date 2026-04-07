#ifndef GIAC_BRIDGE_H
#define GIAC_BRIDGE_H

#include <string>

class GiacBridge {
public:
    // Inicializa el motor (limpia memoria, prepara variables)
    static bool begin();
    
    // Envía una expresión y devuelve el resultado en texto
    static std::string evaluate(std::string expression);
};

#endif