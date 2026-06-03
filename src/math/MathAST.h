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
 * MathAST.h — Abstract Syntax Tree para Motor Matemático V.P.A.M.
 *
 * Fase 1+5: Estructuras de datos del AST y sistema de Layout Recursivo.
 *
 * REGLA DE ORO: La expresión NUNCA se representa como un String lineal.
 * Todo (input, UI, cálculo) se basa al 100% en este árbol dinámico.
 *
 * Jerarquía de nodos:
 *   MathNode (base abstracta)
 *   ├── NodeRow             Secuencia horizontal de hijos [h1, h2, ...]
 *   ├── NodeNumber          Literal numérico: "42", "3.14"
 *   ├── NodeOperator        Operador binario: +, −, ×
 *   ├── NodeEmpty           Placeholder □ (donde el usuario debe escribir)
 *   ├── NodeFraction        Fracción apilada: numerador / denominador
 *   ├── NodePower           Superíndice: base^exponente
 *   ├── NodeRoot            Radical: √(contenido) o ⁿ√(contenido)
 *   ├── NodeParen           Grupo entre paréntesis: (contenido)
 *   ├── NodeFunction        Función: sin(x), cos(x), ln(x), log(x), etc.
 *   ├── NodeLogBase         Logaritmo base custom: log_n(x) con subíndice
 *   ├── NodeConstant        Constante algebraica: π, e
 *   └── NodePeriodicDecimal Decimal periódico: 0.̄3̄ con overline
 *
 * Layout:
 *   Cada nodo expone calculateLayout(FontMetrics) que calcula recursivamente
 *   { width, ascent, descent }. El baseline queda a 'ascent' píxeles del tope.
 *
 *   ascent   ↑  distancia del baseline hacia ARRIBA (positivo)
 *   descent  ↓  distancia del baseline hacia ABAJO  (positivo)
 *   height() =  ascent + descent
 *
 * Dependencias: C++ estándar únicamente (sin LVGL, sin Arduino).
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "MathTypography.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Forward declarations
// ════════════════════════════════════════════════════════════════════════════
class MathNode;
using NodePtr = std::unique_ptr<MathNode>;

// ════════════════════════════════════════════════════════════════════════════
// NodeType — Identifica el tipo concreto de cada nodo
// ════════════════════════════════════════════════════════════════════════════
enum class NodeType : uint8_t {
    Row,
    Number,
    Operator,
    Empty,
    Fraction,
    Power,
    Root,
    Paren,
    Function,         // sin, cos, tan, arcsin, arccos, arctan, ln, log
    LogBase,          // log_n(x) con subíndice VPAM
    Constant,         // π, e
    Variable,         // Variable: x, y, z, A-F, Ans, PreAns
    PeriodicDecimal,  // Decimal periódico con overline (solo resultado)
    DefIntegral,      // Definite integral: ∫[lower,upper] expr d(var)
    Summation,        // Summation series: ∑[lower,upper] expr (var=n)
    Subscript,        // Generic subscript: base_subscript (e.g. x₁, x₂)
    BigOp,            // Generic large operator: ∏, ⋂, ⋃ (style-dependent limits)
};

// ════════════════════════════════════════════════════════════════════════════
// OpKind — Tipo de operador binario
// ════════════════════════════════════════════════════════════════════════════
enum class OpKind : uint8_t {
    Add,       // +
    Sub,       // −  (resta, no signo negativo)
    Mul,       // ×
    PlusMinus, // ±  (plus-minus, for quadratic formula)
};

// ════════════════════════════════════════════════════════════════════════════
// FuncKind — Tipo de función matemática
// ════════════════════════════════════════════════════════════════════════════
enum class FuncKind : uint8_t {
    Sin,       // sin
    Cos,       // cos
    Tan,       // tan
    ArcSin,    // sin⁻¹  (arcsin)
    ArcCos,    // cos⁻¹  (arccos)
    ArcTan,    // tan⁻¹  (arctan)
    Ln,        // ln     (logaritmo natural)
    Log,       // log    (logaritmo base 10)
};

// ════════════════════════════════════════════════════════════════════════════
// BigOpKind — Tipo de operador grande (n-ary operator)
// ════════════════════════════════════════════════════════════════════════════
enum class BigOpKind : uint8_t {
    Product,       // ∏
    CoProduct,     // ∐
    Intersection,  // ⋂
    Union,         // ⋃
    LogicalAnd,    // ⋀
    LogicalOr,     // ⋁
};

// ════════════════════════════════════════════════════════════════════════════
// ConstKind — Tipo de constante algebraica
// ════════════════════════════════════════════════════════════════════════════
enum class ConstKind : uint8_t {
    Pi,    // π
    E,     // e (Euler)
    Imag,  // i (imaginary unit)
};

// ════════════════════════════════════════════════════════════════════════════
// FontMetrics — Métricas tipográficas para el cálculo de layout
//
// Son independientes de LVGL/TFT; se inyectan desde fuera.
// En Fase 3 se extraerán de lv_font_t.
// ════════════════════════════════════════════════════════════════════════════
struct FontMetrics {
    int16_t charWidth;   ///< Ancho promedio de un dígito (monoespaciado)
    int16_t ascent;      ///< Píxeles del baseline al tope de la línea (line_height)
    int16_t descent;     ///< Píxeles del baseline a la parte baja (≥0)
    int16_t capHeight;   ///< Píxeles del baseline al tope de las mayúsculas (Cap Height)
    uint8_t scriptLevel = 0;             ///< 0 = base, 1 = script
    const FontMetrics* script = nullptr; ///< Métricas del nivel de script (Level 1)
    int16_t emSize = 18;                 ///< Font size in pixels (for duToPx / muToPx conversion)
    MathStyle style = MathStyle::DISPLAY_STYLE; ///< Current math typesetting context

    /// Altura total de línea
    int16_t height() const { return ascent + descent; }

    /// Eje matemático: donde van las barras de fracción y el centro de +/−.
    /// ≈ mitad del ascent (centro vertical del dígito).
    int16_t axisHeight() const { return ascent / 2; }

    /// True if this is a display or text (non-script) style.
    bool isDisplayStyle() const {
        return style == MathStyle::DISPLAY_STYLE;
    }

    /// Métricas reducidas para superíndices/subíndices.
    ///
    /// Level 1 (script): scales by scriptPercentScaleDown (70% for STIX Two Math).
    /// Level 2 (scriptscript): scales by scriptScriptPercentScaleDown (55%).
    ///
    /// When a pre-computed script FontMetrics is already attached (e.g. from
    /// metricsFromFont for the 12pt font), it is returned directly for level 1;
    /// level 2 falls through to the MathConstantsProvider-based scaling.
    FontMetrics superscript() const {
        using MCP = MathConstantsProvider;

        if (scriptLevel == 0 && script) {
            // Level 0→1: use the pre-computed script font metrics
            FontMetrics out = *script;
            out.scriptLevel = 1;
            out.style = MathStyle::SCRIPT;
            out.script = nullptr;  // prevent infinite recursion
            return out;
        }

        // Level 1→2 or fallback: scale by the appropriate MATH constant
        int16_t scalePercent = (scriptLevel >= 1)
            ? MCP::scriptScriptPercentScaleDown()   // 55% for scriptscript
            : MCP::scriptPercentScaleDown();         // 70% for script

        auto scale = [scalePercent](int16_t v, int16_t mn) -> int16_t {
            int16_t r = static_cast<int16_t>((static_cast<int32_t>(v) * scalePercent) / 100);
            return r < mn ? mn : r;
        };

        FontMetrics out;
        out.charWidth  = scale(charWidth, 6);
        out.ascent     = scale(ascent, 8);
        out.descent    = scale(descent, 1);
        out.capHeight  = scale(capHeight, 8);
        out.scriptLevel = static_cast<uint8_t>(scriptLevel + 1);
        out.emSize     = scale(emSize, 12);
        out.style      = (scriptLevel >= 1) ? MathStyle::SCRIPTSCRIPT : MathStyle::SCRIPT;
        out.script     = nullptr;
        return out;
    }
};

/// Métricas por defecto razonables (≈ STIX Two Math 18 a ~10 px de ancho).
inline FontMetrics defaultFontMetrics() {
    return { 10, 14, 3, 10, 0, nullptr, 18, MathStyle::DISPLAY_STYLE };
}

// ════════════════════════════════════════════════════════════════════════════
// LayoutResult — Resultado geométrico del layout de un nodo
// ════════════════════════════════════════════════════════════════════════════
struct LayoutResult {
    int16_t width   = 0;   ///< Ancho total en píxeles
    int16_t ascent  = 0;   ///< Píxeles sobre el baseline (positivo = arriba)
    int16_t descent = 0;   ///< Píxeles bajo el baseline  (positivo = abajo)

    /// Altura total = ascent + descent
    int16_t height() const { return ascent + descent; }

    /// Distancia del tope de la bounding box al baseline
    int16_t baseline() const { return ascent; }
};

// ════════════════════════════════════════════════════════════════════════════
// MathNode — Clase base abstracta de todos los nodos del AST
// ════════════════════════════════════════════════════════════════════════════
class MathNode {
public:
    virtual ~MathNode() = default;

    // ── PSRAM allocation — all AST nodes go to external RAM ──
    void* operator new(std::size_t size);
    void  operator delete(void* ptr) noexcept;

    // ── Identidad ──
    NodeType type() const { return _type; }

    // ── TeX Math Class (for inter-atom spacing) ──
    /// Returns the TeX atom class of this node.
    /// Default is ORD (ordinary). Override in nodes that need different spacing.
    virtual MathClass mathClass() const { return MathClass::ORD; }

    // ── Script level ──
    uint8_t scriptLevel() const { return _scriptLevel; }
    void setScriptLevel(uint8_t level) { _scriptLevel = (level > 1) ? 1 : level; }

    // ── Navegación por el árbol (padre no-owning) ──
    MathNode* parent() const    { return _parent; }
    void setParent(MathNode* p) { _parent = p; }

    // ── Layout ──
    const LayoutResult& layout() const { return _layout; }

    /**
     * Calcula recursivamente {width, ascent, descent} para este nodo
     * y todos sus descendientes.
     * @param fm  Métricas tipográficas del contexto actual.
     */
    virtual void calculateLayout(const FontMetrics& fm) = 0;

    // ── Hijos (para navegación genérica del cursor) ──
    virtual int        childCount()          const { return 0; }
    virtual MathNode*  child(int /*index*/)  const { return nullptr; }

protected:
    explicit MathNode(NodeType t) : _type(t), _parent(nullptr), _scriptLevel(0) {}

    NodeType     _type;
    MathNode*    _parent;
    uint8_t      _scriptLevel;
    LayoutResult _layout;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeRow — Secuencia horizontal de nodos hijos
//
// Es el "contenedor por defecto". Las ranuras de FractionNode, PowerNode,
// RootNode y ParenNode son todas NodeRow, lo que permite insertar
// múltiples nodos dentro de cualquier ranura.
// ════════════════════════════════════════════════════════════════════════════
class NodeRow : public MathNode {
public:
    NodeRow();

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override;  // Propagates from first non-empty child

    // ── Hijos ──
    int       childCount()        const override;
    MathNode* child(int index)    const override;

    const std::vector<NodePtr>& children() const { return _children; }
    bool isEmpty() const { return _children.empty(); }

    // ── Mutación ──
    void    appendChild(NodePtr node);
    void    insertChild(int index, NodePtr node);
    NodePtr removeChild(int index);
    void    replaceChild(int index, NodePtr node);
    void    clear();

    /// Separación horizontal (px) entre hijos consecutivos
    static constexpr int16_t CHILD_GAP = 1;

private:
    std::vector<NodePtr> _children;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeNumber — Literal numérico (secuencia de dígitos + punto decimal)
//
// Almacena la representación textual: "42", "3.14", "0".
// El cursor (Fase 2) podrá moverse por dentro carácter a carácter.
// ════════════════════════════════════════════════════════════════════════════
class NodeNumber : public MathNode {
public:
    explicit NodeNumber(const std::string& value = "");

    void calculateLayout(const FontMetrics& fm) override;

    // ── Acceso al valor ──
    const std::string& value() const { return _value; }
    void setValue(const std::string& v) { _value = v; }

    // ── Edición carácter a carácter ──
    void appendChar(char c);
    void deleteLastChar();
    bool hasDecimalPoint() const;
    int  length() const { return static_cast<int>(_value.size()); }

private:
    std::string _value;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeOperator — Operador binario: +, −, ×
// ════════════════════════════════════════════════════════════════════════════
class NodeOperator : public MathNode {
public:
    explicit NodeOperator(OpKind op);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::BINARY; }  // All ops are binary

    OpKind      op()     const { return _op; }
    const char* symbol() const;

    /// @deprecated Padding horizontal a cada lado del símbolo del operador (px).
    /// Use MathConstantsProvider::muToPx() with kSpacingTable for TeX spacing.
    static constexpr int16_t OP_PAD = 2;

private:
    OpKind _op;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeEmpty — Placeholder □ ("cuadrado vacío")
//
// Marca una posición donde el usuario aún no ha escrito nada.
// Se renderiza como un rectángulo tenue/punteado.
// ════════════════════════════════════════════════════════════════════════════
class NodeEmpty : public MathNode {
public:
    NodeEmpty();

    void calculateLayout(const FontMetrics& fm) override;

    /// Tamaño visual mínimo del placeholder
    static constexpr int16_t MIN_WIDTH  = 11;
    static constexpr int16_t MIN_HEIGHT = 13;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeFraction — Fracción vertical: numerador / denominador
//
// Ambos hijos son NodeRow (pueden contener múltiples nodos).
// Constructor por defecto → dos NodeRow con un NodeEmpty cada uno.
//
// Layout:
//   ┌──────────────┐  ← tope: ascent desde baseline
//   │  numerador    │
//   │───────────────│  ← barra sobre el eje matemático
//   │  denominador  │
//   └──────────────┘  ← fondo: descent desde baseline
// ════════════════════════════════════════════════════════════════════════════
class NodeFraction : public MathNode {
public:
    NodeFraction();
    NodeFraction(NodePtr numerator, NodePtr denominator);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    // ── Acceso directo ──
    MathNode* numerator()   const { return _numerator.get(); }
    MathNode* denominator() const { return _denominator.get(); }

    void setNumerator(NodePtr node);
    void setDenominator(NodePtr node);

    // ── @deprecated Geometry constants — use MathConstantsProvider instead ──
    // These remain for MathRenderer backward-compatibility (Phase 4 will remove them).
    static constexpr int16_t BAR_THICK = 1;   ///< @deprecated Grosor de la barra (px)
    static constexpr int16_t BAR_H_PAD = 2;   ///< @deprecated Padding horizontal barra↔borde
    static constexpr int16_t BAR_V_GAP = 1;   ///< @deprecated Espacio vertical barra↔contenido

private:
    NodePtr _numerator;
    NodePtr _denominator;

    /// Crea un NodeRow con un NodeEmpty dentro
    static NodePtr makeEmptySlot();
};

// ════════════════════════════════════════════════════════════════════════════
// NodePower — Potencia: base^exponente
//
// base     → NodeRow (contenido capturado, ej. el "3" de "2+3^")
// exponent → NodeRow (superíndice, se renderiza en fuente reducida)
//
// Layout:
//   ┌base┐┌exp┐   El exponente se eleva al ~45% del ascent de la base.
//   │ 23 ││ 4 │   La fuente del exponente es ~70% de la normal.
//   └────┘└───┘
// ════════════════════════════════════════════════════════════════════════════
class NodePower : public MathNode {
public:
    NodePower();
    NodePower(NodePtr base, NodePtr exponent);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()     const { return _base.get(); }
    MathNode* exponent() const { return _exponent.get(); }

    void setBase(NodePtr node);
    void setExponent(NodePtr node);

    /// @deprecated — use MathConstantsProvider::superscriptShiftUp() instead.
    static constexpr int16_t EXP_RAISE_NUM = 1;   // numerador
    static constexpr int16_t EXP_RAISE_DEN = 2;   // denominador → 1/2 = 50% de capHeight

private:
    NodePtr _base;
    NodePtr _exponent;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeRoot — Radical: √(contenido) o ⁿ√(contenido)
//
// radicand → NodeRow (contenido bajo el radical)
// degree   → NodeRow (índice opcional, ej. ³√ — nullptr para raíz cuadrada)
//
// Layout:
//   ┌degree─┐
//   │   ╱‾‾‾‾‾‾‾‾┐   overline
//   │  ╱ radicand │
//   │ ╱           │
//   └╱────────────┘
//    hook  slope
// ════════════════════════════════════════════════════════════════════════════
class NodeRoot : public MathNode {
public:
    NodeRoot();
    explicit NodeRoot(NodePtr radicand, NodePtr degree = nullptr);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }

    int       childCount()     const override;
    MathNode* child(int index) const override;

    MathNode* radicand()  const { return _radicand.get(); }
    MathNode* degree()    const { return _degree.get(); }
    bool      hasDegree() const { return _degree != nullptr; }

    void setRadicand(NodePtr node);
    void setDegree(NodePtr node);

    // ── @deprecated Geometría del símbolo √ — use MathConstantsProvider ──
    static constexpr int16_t HOOK_W       = 3;   ///< Ancho del gancho pequeño
    static constexpr int16_t SLOPE_W      = 6;   ///< Ancho de la línea ascendente
    static constexpr int16_t OVERLINE_GAP = 2;   ///< @deprecated spacing
    static constexpr int16_t OVERLINE_T   = 1;   ///< @deprecated — use provider
    static constexpr int16_t RIGHT_PAD    = 2;   ///< Padding derecho tras contenido

private:
    NodePtr _radicand;
    NodePtr _degree;   ///< nullptr → raíz cuadrada
};

// ════════════════════════════════════════════════════════════════════════════
// NodeParen — Grupo entre paréntesis: ( contenido )
//
// content → NodeRow
//
// Los paréntesis se estiran verticalmente para cubrir la altura del
// contenido (como en la notación matemática real).
// ════════════════════════════════════════════════════════════════════════════
class NodeParen : public MathNode {
public:
    NodeParen();
    explicit NodeParen(NodePtr content);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::INNER; }  // TeXbook Rule 15: parenthesized subformula is INNER

    int       childCount()     const override { return 1; }
    MathNode* child(int index) const override;

    MathNode* content() const { return _content.get(); }
    void setContent(NodePtr node);

    /// Dynamic paren width computed during layout (px).
    /// Replaces the legacy PAREN_W constant. Valid after calculateLayout().
    int16_t parenWidth() const { return _parenWidth; }

    /// @deprecated — use parenWidth() instead for layout-accurate width.
    /// Kept for backward-compatibility during Phase 4 migration.
    static constexpr int16_t PAREN_W    = 5;
    static constexpr int16_t INNER_PAD  = 1;
    static constexpr int16_t VERT_PAD   = 1;

private:
    NodePtr _content;
    int16_t _parenWidth = PAREN_W;  ///< Dynamic delimiter width (replaces PAREN_W)
};

// ════════════════════════════════════════════════════════════════════════════
// NodeFunction — Función matemática: sin(x), cos(x), ln(x), etc.
//
// Visualmente: texto de la función + paréntesis automáticos + contenido.
//   argument → NodeRow (contenido dentro de los paréntesis)
//
// Layout:
//   ┌────────┐┌───────────────┐
//   │ sin    ││(  argument   )│
//   └────────┘└───────────────┘
//   label       auto-paren
//
// El paréntesis se dibuja automáticamente (NO es un NodeParen hijo),
// para mantener la semántica de "función aplicada a argumento".
// ════════════════════════════════════════════════════════════════════════════
class NodeFunction : public MathNode {
public:
    NodeFunction();
    explicit NodeFunction(FuncKind kind, NodePtr argument = nullptr);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 1; }
    MathNode* child(int index) const override;

    FuncKind    funcKind()  const { return _kind; }
    MathNode*   argument()  const { return _argument.get(); }
    const char* label()     const;   ///< "sin", "cos", "ln", etc.

    void setArgument(NodePtr node);

    /// Ancho del paréntesis automático (px)
    static constexpr int16_t PAREN_W   = 5;
    static constexpr int16_t INNER_PAD = 1;
    static constexpr int16_t VERT_PAD  = 1;
    /// Gap entre el texto de la función y el paréntesis abierto
    static constexpr int16_t LABEL_GAP = 0;

private:
    FuncKind _kind;
    NodePtr  _argument;   ///< Contenido del argumento (NodeRow)

    int16_t _labelWidth;  ///< Ancho del texto de la etiqueta (calculado)
};

// ════════════════════════════════════════════════════════════════════════════
// NodeLogBase — Logaritmo de base custom: log_n(x)
//
// Visualmente: "log" + subíndice (base) + paréntesis(argumento)
//   base     → NodeRow (subíndice, ej. "2" para log₂)
//   argument → NodeRow (contenido entre paréntesis)
//
// Layout:
//   ┌─────┐┌base┐┌─────────────┐
//   │ log ││ 2  ││( argument  )│
//   └─────┘└────┘└─────────────┘
//           subscript
//
// El subíndice se renderiza en fuente reducida (~70%) BAJADA respecto
// al baseline, como la lógica inversa del NodePower (superíndice).
// ════════════════════════════════════════════════════════════════════════════
class NodeLogBase : public MathNode {
public:
    NodeLogBase();
    explicit NodeLogBase(NodePtr base, NodePtr argument = nullptr);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()     const { return _base.get(); }
    MathNode* argument() const { return _argument.get(); }

    void setBase(NodePtr node);
    void setArgument(NodePtr node);

    /// Ratio de descenso del subíndice (análogo inverso a EXP_RAISE en Power)
    static constexpr int16_t SUB_DROP_NUM = 1;   // numerador
    static constexpr int16_t SUB_DROP_DEN = 3;   // denominador → 1/3 = 33%

    /// Ancho del paréntesis automático (px)
    static constexpr int16_t PAREN_W   = 5;
    static constexpr int16_t INNER_PAD = 1;
    static constexpr int16_t VERT_PAD  = 1;
    static constexpr int16_t LABEL_GAP = 0;

private:
    NodePtr _base;       ///< Subíndice (base del log)
    NodePtr _argument;   ///< Argumento entre paréntesis

    int16_t _labelWidth; ///< Ancho de "log" (calculado)
};

// ════════════════════════════════════════════════════════════════════════════
// NodeConstant — Constante algebraica: π, e
//
// Se renderiza como un solo símbolo. En la evaluación (Parte 2) se trata
// como variable algebraica para permitir simplificaciones (3*π, e²).
// ════════════════════════════════════════════════════════════════════════════
class NodeConstant : public MathNode {
public:
    explicit NodeConstant(ConstKind kind = ConstKind::Pi);

    void calculateLayout(const FontMetrics& fm) override;

    ConstKind   constKind() const { return _kind; }
    const char* symbol()    const;   ///< "π" o "e"

private:
    ConstKind _kind;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeVariable — Variable algebraica: x, y, z, A-F, Ans, PreAns
//
// Se renderiza como texto (nombre), con estilo visual especial:
//   · x, y, z → azul (#4A90D9), cursiva si la fuente lo soporta
//   · A-F     → color normal, texto en mayúscula
//   · Ans     → bloque "Ans"
//   · PreAns  → bloque "PreAns"
//
// En evaluación, pide su valor al VariableManager::instance().
//
// El char 'name_' identifica la variable:
//   '#' = Ans, '$' = PreAns, 'A'-'F', 'x', 'y', 'z'
// ════════════════════════════════════════════════════════════════════════════
class NodeVariable : public MathNode {
public:
    explicit NodeVariable(char name = 'x');

    void calculateLayout(const FontMetrics& fm) override;

    char        name()  const { return _name; }
    const char* label() const;   ///< "x", "A", "Ans", "PreAns", etc.

    /// ¿Es una variable de función (x, y, z)?
    bool isFunctionVar() const { return _name == 'x' || _name == 'y' || _name == 'z'; }

    /// ¿Es Ans o PreAns?
    bool isAnsVar() const { return _name == '#' || _name == '$'; }

private:
    char _name;
    int16_t _labelWidth;   ///< Ancho calculado del label
};

// ════════════════════════════════════════════════════════════════════════════
// NodePeriodicDecimal — Decimal periódico con overline (SOLO RESULTADO)
//
// Representación visual de un decimal periódico: ej. 0.1̄6̄ o 0.̄3̄
//
// Estructura: integerPart . nonRepeating  repeating
//                                          ‾‾‾‾‾‾‾‾ ← overline
//
// Ejemplo: 1/6 = 0.1666... → intPart="0", nonRepeat="1", repeat="6"
// Ejemplo: 1/3 = 0.333...  → intPart="0", nonRepeat="",  repeat="3"
// Ejemplo: 1/7 = 0.142857142857... → intPart="0", nonRepeat="", repeat="142857"
//
// Este nodo es generado EXCLUSIVAMENTE por el evaluador para el modo
// periódico (Modo 2 S⇔D). No se crea por el usuario.
// ════════════════════════════════════════════════════════════════════════════
class NodePeriodicDecimal : public MathNode {
public:
    NodePeriodicDecimal(const std::string& intPart,
                        const std::string& nonRepeat,
                        const std::string& repeat,
                        bool negative = false);

    void calculateLayout(const FontMetrics& fm) override;

    const std::string& intPart()    const { return _intPart; }
    const std::string& nonRepeat()  const { return _nonRepeat; }
    const std::string& repeat()     const { return _repeat; }
    bool               isNegative() const { return _negative; }

    /// Grosor de la overline sobre los dígitos periódicos
    static constexpr int16_t OVERLINE_T   = 1;
    /// Espacio entre tope de dígitos y la overline
    static constexpr int16_t OVERLINE_GAP = 1;

private:
    std::string _intPart;     ///< Parte entera: "0", "1", "123"
    std::string _nonRepeat;   ///< Dígitos no periódicos tras el punto
    std::string _repeat;      ///< Patrón que se repite (con overline)
    bool        _negative;    ///< ¿Valor negativo?
};

// ════════════════════════════════════════════════════════════════════════════
// NodeDefIntegral — Integral definida: ∫_a^b f(x) dx
//
// 4 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "0")
//   upper    → Límite superior (ej. "1")
//   body     → Expresión integrando (ej. "x^2+1")
//   variable → Variable de integración (ej. "x")
//
// Layout:
//   ┌ upper ┐
//   │  ∫    │ body  d(var)
//   └ lower ┘
//
// El símbolo ∫ se dibuja como vectores (curva S) con la API de LVGL.
// Los límites se renderizan en fuente reducida arriba y abajo del símbolo.
// ════════════════════════════════════════════════════════════════════════════
class NodeDefIntegral : public MathNode {
public:
    NodeDefIntegral();
    NodeDefIntegral(NodePtr lower, NodePtr upper, NodePtr body, NodePtr variable);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }  // Large operator

    int       childCount()     const override { return 4; }
    MathNode* child(int index) const override;

    MathNode* lower()    const { return _lower.get(); }
    MathNode* upper()    const { return _upper.get(); }
    MathNode* body()     const { return _body.get(); }
    MathNode* variable() const { return _variable.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);
    void setVariable(NodePtr node);

    // ── @deprecated Geometry constants ──
    static constexpr int16_t SYMBOL_W      = 12;  ///< Width of the ∫ symbol
    static constexpr int16_t SYMBOL_H_PAD  = 4;   ///< Extra height above/below body
    static constexpr int16_t LIMIT_GAP     = 2;   ///< Gap between symbol and limits
    static constexpr int16_t BODY_GAP      = 3;   ///< Gap between symbol+limits and body
    static constexpr int16_t D_GAP         = 2;   ///< Gap before "d" text

private:
    NodePtr _lower;
    NodePtr _upper;
    NodePtr _body;
    NodePtr _variable;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeSummation — Sumatorio: ∑_{n=a}^{b} expr
//
// 4 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "n=1")
//   upper    → Límite superior (ej. "10")
//   body     → Expresión a sumar (ej. "n^2")
//   variable → Variable del sumatorio (ej. "n")
//
// Layout:
//   ┌ upper ┐
//   │  ∑    │ body
//   └ lower ┘
//
// El símbolo ∑ se dibuja como vectores (forma de zigzag) con LVGL.
// Los límites se renderizan en fuente reducida arriba y abajo.
// ════════════════════════════════════════════════════════════════════════════
class NodeSummation : public MathNode {
public:
    NodeSummation();
    NodeSummation(NodePtr lower, NodePtr upper, NodePtr body, NodePtr variable);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }  // Large operator

    int       childCount()     const override { return 4; }
    MathNode* child(int index) const override;

    MathNode* lower()    const { return _lower.get(); }
    MathNode* upper()    const { return _upper.get(); }
    MathNode* body()     const { return _body.get(); }
    MathNode* variable() const { return _variable.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);
    void setVariable(NodePtr node);

    // ── @deprecated Geometry constants ──
    static constexpr int16_t SYMBOL_W      = 14;  ///< Width of the ∑ symbol
    static constexpr int16_t SYMBOL_H_PAD  = 4;   ///< Extra height above/below body
    static constexpr int16_t LIMIT_GAP     = 2;   ///< Gap between symbol and limits
    static constexpr int16_t BODY_GAP      = 3;   ///< Gap between symbol+limits and body

private:
    NodePtr _lower;
    NodePtr _upper;
    NodePtr _body;
    NodePtr _variable;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeSubscript — Generic subscript: base_subscript (e.g., x₁, x₂)
//
// Visually: base + subscript bajado respecto al baseline
//   base      → NodeRow (contenido principal, ej. "x")
//   subscript → NodeRow (subíndice, ej. "1", "2")
//
// Layout:
//   ┌base┐┌sub┐   El subíndice se baja ~33% del descent (como NodeLogBase)
//   │ x  ││ 1 │   La fuente del subíndice es ~70% de la normal.
//   └────┘└───┘
//
// Used by the Educational Tutor Engine for x₁, x₂ in quadratic solutions.
// ════════════════════════════════════════════════════════════════════════════
class NodeSubscript : public MathNode {
public:
    NodeSubscript();
    NodeSubscript(NodePtr base, NodePtr subscript);

    void calculateLayout(const FontMetrics& fm) override;

    int       childCount()     const override { return 2; }
    MathNode* child(int index) const override;

    MathNode* base()      const { return _base.get(); }
    MathNode* subscript() const { return _subscript.get(); }

    void setBase(NodePtr node);
    void setSubscript(NodePtr node);

    /// Ratio de descenso del subíndice (mirrors NodeLogBase)
    static constexpr int16_t SUB_DROP_NUM = 1;   // numerador
    static constexpr int16_t SUB_DROP_DEN = 3;   // denominador → 1/3 = 33%

private:
    NodePtr _base;
    NodePtr _subscript;
};

// ════════════════════════════════════════════════════════════════════════════
// NodeBigOp — Generic large n-ary operator: ∏, ⋂, ⋃, ⋀, ⋁
//
// 3 hijos, todos NodeRow:
//   lower    → Límite inferior (ej. "i=1")
//   upper    → Límite superior (ej. "n")
//   body     → Expresión (ej. "x_i")
//
// Style-dependent limit positioning (OpenType Spec §4.5, TeXbook Rule 13):
//   MathStyle::DISPLAY:
//     Limits centered above/below the operator.
//     Uses a larger size variant if glyph advance ≥ DisplayOperatorMinHeight.
//     Layout:
//       ┌ upper ┐
//       │  ∏    │ body
//       └ lower ┘
//
//   MathStyle::TEXT / SCRIPT / SCRIPTSCRIPT:
//     Limits as sub/superscript to the RIGHT of the operator (inline form).
//     Layout:
//       ∏_{lower}^{upper} body
//     (lower=subscript, upper=superscript, both to the right)
// ════════════════════════════════════════════════════════════════════════════
class NodeBigOp : public MathNode {
public:
    NodeBigOp();
    NodeBigOp(BigOpKind kind, NodePtr lower, NodePtr upper, NodePtr body);

    void calculateLayout(const FontMetrics& fm) override;
    MathClass mathClass() const override { return MathClass::OP; }

    int       childCount()     const override { return 3; }
    MathNode* child(int index) const override;

    BigOpKind bigOpKind() const { return _kind; }
    uint32_t  operatorCodepoint() const;  ///< Unicode codepoint for the operator glyph
    const char* operatorUtf8() const;     ///< UTF-8 string for the operator glyph

    MathNode* lower()   const { return _lower.get(); }
    MathNode* upper()   const { return _upper.get(); }
    MathNode* body()    const { return _body.get(); }

    void setLower(NodePtr node);
    void setUpper(NodePtr node);
    void setBody(NodePtr node);

    /// True if the operator glyph is tall enough for display-style limits (Spec §4.5).
    bool useDisplayLimits() const { return _useDisplayLimits; }

private:
    BigOpKind _kind;
    uint32_t  _operatorCp;        ///< Unicode codepoint for rendering
    NodePtr   _lower;
    NodePtr   _upper;
    NodePtr   _body;
    bool      _useDisplayLimits;  ///< Computed during layout
};

// ════════════════════════════════════════════════════════════════════════════
// Factory helpers — Creación rápida de nodos
// ════════════════════════════════════════════════════════════════════════════
NodePtr makeRow();
NodePtr makeNumber(const std::string& value);
NodePtr makeOperator(OpKind op);
NodePtr makeEmpty();

/// Fracción con numerador y denominador opcionales (nullptr → slot vacío)
NodePtr makeFraction(NodePtr num = nullptr, NodePtr den = nullptr);

/// Potencia con base y exponente opcionales (nullptr → slot vacío)
NodePtr makePower(NodePtr base = nullptr, NodePtr exp = nullptr);

/// Raíz con radicando y grado opcionales (nullptr → slot vacío / raíz cuadrada)
NodePtr makeRoot(NodePtr radicand = nullptr, NodePtr degree = nullptr);

/// Paréntesis con contenido opcional (nullptr → slot vacío)
NodePtr makeParen(NodePtr content = nullptr);

/// Función matemática con argumento opcional
NodePtr makeFunction(FuncKind kind, NodePtr argument = nullptr);

/// Logaritmo de base custom con base y argumento opcionales
NodePtr makeLogBase(NodePtr base = nullptr, NodePtr argument = nullptr);

/// Constante algebraica (π o e)
NodePtr makeConstant(ConstKind kind);

/// Variable algebraica (x, y, z, A-F, Ans, PreAns)
NodePtr makeVariable(char name);

/// Decimal periódico (solo para resultados)
NodePtr makePeriodicDecimal(const std::string& intPart,
                            const std::string& nonRepeat,
                            const std::string& repeat,
                            bool negative = false);

/// Integral definida con límites, cuerpo y variable opcionales
NodePtr makeDefIntegral(NodePtr lower = nullptr, NodePtr upper = nullptr,
                        NodePtr body = nullptr, NodePtr variable = nullptr);

/// Subscript genérico: base con subíndice (ej. x₁, x₂)
NodePtr makeSubscript(NodePtr base = nullptr, NodePtr subscript = nullptr);

/// Sumatorio con límites, cuerpo y variable opcionales
NodePtr makeSummation(NodePtr lower = nullptr, NodePtr upper = nullptr,
                      NodePtr body = nullptr, NodePtr variable = nullptr);

/// Large n-ary operator (∏, ⋂, ⋃, etc.) with style-dependent limits
NodePtr makeBigOp(BigOpKind kind, NodePtr lower = nullptr,
                  NodePtr upper = nullptr, NodePtr body = nullptr);

// ════════════════════════════════════════════════════════════════════════════
// Debug — Volcado legible del árbol
//
// Ejemplo de salida:
//   Row
//     Number "2"
//     Operator "+"
//     Fraction
//       num: Row
//         Number "3"
//       den: Row
//         Empty □
// ════════════════════════════════════════════════════════════════════════════
std::string dumpTree(const MathNode* node, int indent = 0);

// ════════════════════════════════════════════════════════════════════════════
// Deep clone — Copia profunda de un subárbol AST
//
// Devuelve un nuevo árbol independiente. Los punteros parent se actualizan
// correctamente en la copia. Útil para el sistema de historial.
// ════════════════════════════════════════════════════════════════════════════
NodePtr cloneNode(const MathNode* node);

} // namespace vpam
