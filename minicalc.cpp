#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

struct Posicion {
    Posicion() : linea(1), columna(1) {}
    std::size_t linea;
    std::size_t columna;
};

struct Token {
    Token(const std::string& tipo, const std::string& lexema, Posicion posicion)
        : tipo(tipo), lexema(lexema), posicion(posicion) {}

    std::string tipo;
    std::string lexema;
    Posicion posicion;
};

class ErrorMiniCalc : public std::runtime_error {
public:
    ErrorMiniCalc(Posicion posicion, const std::string& mensaje)
        : std::runtime_error(formatear(posicion, mensaje)), lugar(posicion) {}

    Posicion posicion() const { return lugar; }

private:
    Posicion lugar;

    static std::string formatear(Posicion posicion, const std::string& mensaje) {
        std::ostringstream salida;
        salida << "Error [Linea " << posicion.linea
               << ", Columna " << posicion.columna << "]: " << mensaje;
        return salida.str();
    }
};

enum Estado { I, P, PR, PRI, PRIN, PRINT, ID, NUM, BL, W, S, M };

enum Columna {
    COL_P, COL_R, COL_I, COL_N, COL_T, COL_LETRA,
    COL_DIGITO, COL_BLANCO, COL_SIMBOLO, COL_OTRO, TOTAL_COLUMNAS
};

const Estado transiciones[][TOTAL_COLUMNAS] = {
    /* I     */ {P, ID, ID, ID, ID, ID, NUM, BL, S, M},
    /* P     */ {W, PR, W, W, W, W, W, M, M, M},
    /* PR    */ {W, W, PRI, W, W, W, W, M, M, M},
    /* PRI   */ {W, W, W, PRIN, W, W, W, M, M, M},
    /* PRIN  */ {W, W, W, W, PRINT, W, W, M, M, M},
    /* PRINT */ {W, W, W, W, W, W, W, M, M, M},
    /* ID    */ {W, W, W, W, W, W, W, M, M, M},
    /* NUM   */ {M, M, M, M, M, M, NUM, M, M, M},
    /* BL    */ {M, M, M, M, M, M, M, BL, M, M},
    /* W     */ {W, W, W, W, W, W, W, M, M, M},
    /* S     */ {M, M, M, M, M, M, M, M, M, M},
    /* M     */ {M, M, M, M, M, M, M, M, M, M},
};

const std::string letrasPrint = "print";
const std::string simbolos = "=+-*/();";
const char* const tiposSimbolo[] = {
    "OP_ASSIGN", "OP_ADD", "OP_SUB", "OP_MUL", "OP_DIV",
    "PUNCT_LPAREN", "PUNCT_RPAREN", "PUNCT_SEMI"
};

Columna claseCaracter(char caracter) {
    const std::size_t letra = letrasPrint.find(caracter);
    if (letra != std::string::npos) return static_cast<Columna>(COL_P + letra);
    if ('a' <= caracter && caracter <= 'z') return COL_LETRA;
    if ('0' <= caracter && caracter <= '9') return COL_DIGITO;
    if (caracter == ' ' || caracter == '\t' ||
        caracter == '\r' || caracter == '\n') return COL_BLANCO;
    if (simbolos.find(caracter) != std::string::npos) return COL_SIMBOLO;
    return COL_OTRO;
}

std::string describirCaracter(char caracter) {
    const unsigned char codigo = static_cast<unsigned char>(caracter);
    std::ostringstream salida;
    if (codigo >= 32 && codigo < 127) {
        salida << '\'' << caracter << '\'';
    } else {
        salida << "con codigo " << static_cast<int>(codigo);
    }
    if ('A' <= caracter && caracter <= 'Z') {
        salida << " (solo se permiten letras minusculas)";
    } else if (codigo >= 128) {
        salida << " (no se permiten tildes, enie ni caracteres especiales)";
    }
    return salida.str();
}

class Lexer {
public:
    explicit Lexer(const std::string& fuente) : fuente(fuente), indice(0) {}

    Token siguiente() {
        while (indice < fuente.size()) {
            const std::size_t inicio = indice;
            const Posicion posicionInicial = posicion;
            Estado estado = I;

            while (indice < fuente.size()) {
                const char caracter = fuente[indice];
                const Estado destino = transiciones[estado][claseCaracter(caracter)];
                if (destino == M) break;
                estado = destino;
                avanzarCaracter();
            }

            if (estado == BL) continue;
            const std::string lexema = fuente.substr(inicio, indice - inicio);
            switch (estado) {
                case P:
                case ID: return Token("ID", lexema, posicionInicial);
                case PRINT: return Token("KW_PRINT", lexema, posicionInicial);
                case NUM: return Token("NUM", lexema, posicionInicial);
                case S: return Token(tiposSimbolo[simbolos.find(lexema[0])],
                                lexema, posicionInicial);
                case I:
                    throw ErrorMiniCalc(posicionInicial,
                        "Error lexico. Caracter no permitido " +
                        describirCaracter(fuente[indice]) + ".");
                default:
                    throw ErrorMiniCalc(posicionInicial,
                        "Error lexico. Palabra no reconocida '" + lexema + "'.");
            }
        }
        return Token("EOF", "", posicion);
    }

private:
    const std::string& fuente;
    std::size_t indice;
    Posicion posicion;

    void avanzarCaracter() {
        const char caracter = fuente[indice++];
        if (caracter == '\r' || caracter == '\n') {
            if (caracter == '\r' && indice < fuente.size() &&
                fuente[indice] == '\n') ++indice;
            ++posicion.linea;
            posicion.columna = 1;
        } else {
            ++posicion.columna;
        }
    }
};

// ---------------------------------------------------------------------------
// Fase de ejecucion agregada por Abrahan Baez.
// El mismo analizador por descenso recursivo funciona en dos modos:
//   1) Solo sintaxis: memoria == 0. Verifica la gramatica, como antes.
//   2) Ejecucion: memoria != 0. Cada regla de la gramatica devuelve el valor
//      de su subexpresion (traduccion dirigida por la sintaxis), las
//      asignaciones guardan valores en la tabla de simbolos y print muestra
//      el resultado. Aqui se detectan los errores semanticos: variable sin
//      valor asignado, division por cero y resultado fuera de rango.
// ---------------------------------------------------------------------------
typedef std::map<std::string, double> TablaSimbolos;

std::string formatearNumero(double valor) {
    if (valor == 0) valor = 0;  // evita mostrar "-0"
    std::ostringstream salida;
    salida << std::setprecision(12) << valor;
    return salida.str();
}

bool esFinito(double valor) {
    return valor - valor == 0;  // falso para infinito y NaN (C++98)
}

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens,
                    TablaSimbolos* memoria = 0, std::ostream* salida = 0)
        : tokens(tokens), indice(0), sentencias(0),
          memoria(memoria), salida(salida) {}

    std::size_t programa() {
        lista_sentencias();
        if (actual().tipo != "EOF") {
            error("una asignacion o la instruccion 'print'");
        }
        return sentencias;
    }

private:
    const std::vector<Token>& tokens;
    std::size_t indice;
    std::size_t sentencias;
    TablaSimbolos* memoria;
    std::ostream* salida;

    const Token& actual() const { return tokens[indice]; }

    bool ejecutando() const { return memoria != 0; }

    void errorSemantico(Posicion posicion, const std::string& mensaje) const {
        throw ErrorMiniCalc(posicion, "Error semantico. " + mensaje);
    }

    double comprobarRango(double valor, Posicion posicion) const {
        if (ejecutando() && !esFinito(valor)) {
            errorSemantico(posicion, "El resultado es demasiado grande para representarlo.");
        }
        return valor;
    }

    void error(const std::string& esperado) const {
        const Token& token = actual();
        const std::string encontrado = token.tipo == "EOF"
            ? "el fin del archivo" : "'" + token.lexema + "'";
        throw ErrorMiniCalc(token.posicion,
            "Error sintactico. Se esperaba " + esperado +
            " y se encontro " + encontrado + ".");
    }

    void exigir(const std::string& tipo, const std::string& esperado) {
        if (actual().tipo != tipo) error(esperado);
        ++indice;
    }

    void lista_sentencias() {
        while (actual().tipo == "ID" || actual().tipo == "KW_PRINT") {
            sentencia();
        }
    }

    void sentencia() {
        const Token& inicio = actual();
        ++indice;
        if (inicio.tipo == "ID") {
            exigir("OP_ASSIGN", "'='");
            const double valor = expresion();
            exigir("PUNCT_SEMI", "';'");
            if (ejecutando()) (*memoria)[inicio.lexema] = valor;
        } else {
            const double valor = expresion();
            exigir("PUNCT_SEMI", "';'");
            if (ejecutando() && salida) {
                *salida << "Linea " << inicio.posicion.linea << ": print -> "
                        << formatearNumero(valor) << '\n';
            }
        }
        ++sentencias;
    }

    double expresion() {
        const double valor = cola_expresion(termino());
        const std::string& tipo = actual().tipo;
        if (tipo == "NUM" || tipo == "ID" || tipo == "PUNCT_LPAREN") {
            error("un operador (+, -, * o /)");
        }
        return valor;
    }

    double cola_expresion(double izquierdo) {
        while (actual().tipo == "OP_ADD" || actual().tipo == "OP_SUB") {
            const Token& operador = actual();
            ++indice;
            const double derecho = termino();
            izquierdo = operador.tipo == "OP_ADD" ? izquierdo + derecho
                                                  : izquierdo - derecho;
            comprobarRango(izquierdo, operador.posicion);
        }
        return izquierdo;
    }

    double termino() {
        return cola_termino(factor());
    }

    double cola_termino(double izquierdo) {
        while (actual().tipo == "OP_MUL" || actual().tipo == "OP_DIV") {
            const Token& operador = actual();
            ++indice;
            const double derecho = factor();
            if (operador.tipo == "OP_MUL") {
                izquierdo *= derecho;
            } else if (ejecutando() && derecho == 0) {
                errorSemantico(operador.posicion, "Division por cero.");
            } else if (ejecutando()) {
                izquierdo /= derecho;
            }
            comprobarRango(izquierdo, operador.posicion);
        }
        return izquierdo;
    }

    double factor() {
        const Token& token = actual();
        if (token.tipo == "PUNCT_LPAREN") {
            ++indice;
            const double valor = expresion();
            exigir("PUNCT_RPAREN", "')'");
            return valor;
        }
        if (token.tipo == "NUM") {
            ++indice;
            return comprobarRango(std::strtod(token.lexema.c_str(), 0),
                                  token.posicion);
        }
        if (token.tipo == "ID") {
            ++indice;
            if (!ejecutando()) return 0;
            const TablaSimbolos::const_iterator variable = memoria->find(token.lexema);
            if (variable == memoria->end()) {
                errorSemantico(token.posicion, "La variable '" + token.lexema +
                    "' se usa antes de asignarle un valor.");
            }
            return variable->second;
        }
        error("una variable, un numero o '('");
        return 0;
    }
};

std::string extraerLinea(const std::string& fuente, std::size_t linea) {
    std::size_t inicio = 0;
    for (std::size_t actual = 1; actual < linea; ++actual) {
        std::size_t fin = fuente.find_first_of("\r\n", inicio);
        if (fin == std::string::npos) return "";
        if (fuente[fin] == '\r' && fin + 1 < fuente.size() &&
            fuente[fin + 1] == '\n') ++fin;
        inicio = fin + 1;
    }
    const std::size_t fin = fuente.find_first_of("\r\n", inicio);
    return fuente.substr(inicio, fin == std::string::npos
                                     ? std::string::npos : fin - inicio);
}

void mostrarContexto(std::ostream& salida, const std::string& fuente,
                     Posicion posicion) {
    const std::string linea = extraerLinea(fuente, posicion.linea);
    salida << "  " << linea << "\n  ";
    for (std::size_t i = 1; i < posicion.columna; ++i) {
        salida << (i <= linea.size() && linea[i - 1] == '\t' ? '\t' : ' ');
    }
    salida << "^\n";
}

std::string limpiarRuta(const std::string& ruta) {
    const std::string blancos = " \t\r\n";
    const std::size_t inicio = ruta.find_first_not_of(blancos);
    if (inicio == std::string::npos) return "";
    std::string limpia = ruta.substr(inicio, ruta.find_last_not_of(blancos) - inicio + 1);
    if (limpia.size() >= 2 && limpia[0] == '"' && limpia[limpia.size() - 1] == '"') {
        limpia = limpia.substr(1, limpia.size() - 2);
    }
    return limpia;
}

int analizarArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta.c_str(), std::ios::binary);
    if (!archivo) {
        std::cerr << "No se pudo abrir el archivo: " << ruta << '\n';
        return 2;
    }

    std::string fuente((std::istreambuf_iterator<char>(archivo)),
                       std::istreambuf_iterator<char>());
    if (archivo.bad()) {
        std::cerr << "No se pudo leer el archivo: " << ruta << '\n';
        return 2;
    }

    const std::string bom = "\xEF\xBB\xBF";
    if (fuente.compare(0, bom.size(), bom) == 0) {
        fuente.erase(0, bom.size());
    }

    try {
        std::cout << "=== INICIO DE ANALISIS LEXICO ===\n";
        Lexer lexer(fuente);
        std::vector<Token> tokens;
        Token token = lexer.siguiente();
        while (token.tipo != "EOF") {
            std::cout << '<' << token.tipo << ", " << '"' << token.lexema << '"'
                      << ", " << token.posicion.linea << ", " << token.posicion.columna
                      << ">\n";
            tokens.push_back(token);
            token = lexer.siguiente();
        }
        tokens.push_back(token);
        std::cout << "Total de tokens: " << tokens.size() - 1 << '\n';

        if (tokens.size() == 1) {
            std::cout << "El archivo no contiene instrucciones para analizar.\n";
            return 1;
        }

        std::cout << "=== INICIO DE ANALISIS SINTACTICO ===\n";
        const std::size_t sentencias = Parser(tokens).programa();
        std::cout << "El analisis sintactico finalizo exitosamente sin errores ("
                  << sentencias << (sentencias == 1 ? " sentencia" : " sentencias")
                  << ").\n";

        std::cout << "=== INICIO DE EJECUCION ===\n";
        TablaSimbolos memoria;
        Parser(tokens, &memoria, &std::cout).programa();
        std::cout << "Tabla de simbolos:";
        if (memoria.empty()) std::cout << " (sin variables)";
        std::cout << '\n';
        for (TablaSimbolos::const_iterator variable = memoria.begin();
             variable != memoria.end(); ++variable) {
            std::cout << "  " << variable->first << " = "
                      << formatearNumero(variable->second) << '\n';
        }
        std::cout << "La ejecucion finalizo sin errores.\n";
    } catch (const ErrorMiniCalc& error) {
        std::cout.flush();
        std::cerr << error.what() << '\n';
        mostrarContexto(std::cerr, fuente, error.posicion());
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        return analizarArchivo(limpiarRuta(argv[1]));
    }
    if (argc > 2) {
        std::cerr << "Uso: minicalc <archivo.calc>\n";
        return 2;
    }

    std::string ruta;
    while (true) {
        std::cout << "\nEscribi la ruta del archivo .calc (Enter para salir): ";
        if (!std::getline(std::cin, ruta)) break;
        ruta = limpiarRuta(ruta);
        if (ruta.empty()) break;
        analizarArchivo(ruta);
    }
    return 0;
}
