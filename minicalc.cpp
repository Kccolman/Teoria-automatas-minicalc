#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
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
        : std::runtime_error(formatear(posicion, mensaje)) {}

private:
    static std::string formatear(Posicion posicion, const std::string& mensaje) {
        std::ostringstream salida;
        salida << "Error [Linea " << posicion.linea
               << ", Columna " << posicion.columna << "]: " << mensaje;
        return salida.str();
    }
};

// Las columnas de la matriz corresponden a p, r, i, n, t, otras
// minusculas, digitos, blancos, simbolos y caracteres no permitidos.
enum Estado { I, P, PR, PRI, PRIN, PRINT, ID, NUM, BL, W, S, M };

const Estado transiciones[][10] = {
    /* I     */ {P, ID, ID, ID, ID, ID, NUM, BL, S, M},
    /* P     */ {W, PR, W, W, W, W, M, M, M, M},
    /* PR    */ {W, W, PRI, W, W, W, M, M, M, M},
    /* PRI   */ {W, W, W, PRIN, W, W, M, M, M, M},
    /* PRIN  */ {W, W, W, W, PRINT, W, M, M, M, M},
    /* PRINT */ {W, W, W, W, W, W, M, M, M, M},
    /* ID    */ {W, W, W, W, W, W, M, M, M, M},
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

std::size_t claseCaracter(char caracter) {
    const std::size_t letra = letrasPrint.find(caracter);
    if (letra != std::string::npos) return letra;
    if ('a' <= caracter && caracter <= 'z') return 5;
    if ('0' <= caracter && caracter <= '9') return 6;
    if (caracter == ' ' || caracter == '\t' ||
        caracter == '\r' || caracter == '\n') return 7;
    if (simbolos.find(caracter) != std::string::npos) return 8;
    return 9;
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
                        "Error lexico. Caracter no permitido '" +
                        std::string(1, fuente[indice]) + "'.");
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

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens), indice(0) {}

    void programa() {
        lista_sentencias();
        if (actual().tipo != "EOF") {
            error("una asignacion o la instruccion 'print'");
        }
    }

private:
    const std::vector<Token>& tokens;
    std::size_t indice;

    const Token& actual() const { return tokens[indice]; }

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
        const bool asignacion = actual().tipo == "ID";
        ++indice;
        if (asignacion) exigir("OP_ASSIGN", "'='");
        expresion();
        exigir("PUNCT_SEMI", "';'");
    }

    void expresion() {
        termino();
        cola_expresion();
        const std::string& tipo = actual().tipo;
        if (tipo == "NUM" || tipo == "ID" || tipo == "PUNCT_LPAREN") {
            error("un operador (+, -, * o /)");
        }
    }

    void cola_expresion() {
        while (actual().tipo == "OP_ADD" || actual().tipo == "OP_SUB") {
            ++indice;
            termino();
        }
    }

    void termino() {
        factor();
        cola_termino();
    }

    void cola_termino() {
        while (actual().tipo == "OP_MUL" || actual().tipo == "OP_DIV") {
            ++indice;
            factor();
        }
    }

    void factor() {
        if (actual().tipo == "PUNCT_LPAREN") {
            ++indice;
            expresion();
            exigir("PUNCT_RPAREN", "')'");
        } else if (actual().tipo == "NUM" || actual().tipo == "ID") {
            ++indice;
        } else {
            error("una variable, un numero o '('");
        }
    }
};

int main(int argc, char* argv[]) {
    std::string ruta;
    if (argc == 2) {
        ruta = argv[1];
    } else if (argc == 1) {
        std::cout << "Escribi la ruta del archivo .calc: ";
        if (!std::getline(std::cin, ruta) || ruta.empty()) {
            std::cerr << "Falta la ruta del archivo .calc.\n";
            return 2;
        }
    } else {
        std::cerr << "Uso: minicalc <archivo.calc>\n";
        return 2;
    }

    std::ifstream archivo(ruta.c_str(), std::ios::binary);
    if (!archivo) {
        std::cerr << "No se pudo abrir el archivo: " << ruta << '\n';
        return 2;
    }

    const std::string fuente((std::istreambuf_iterator<char>(archivo)),
                             std::istreambuf_iterator<char>());
    if (archivo.bad()) {
        std::cerr << "No se pudo leer el archivo: " << ruta << '\n';
        return 2;
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

        std::cout << "=== INICIO DE ANALISIS SINTACTICO ===\n";
        Parser(tokens).programa();
        std::cout << "El analisis sintactico finalizo exitosamente sin errores.\n";
    } catch (const ErrorMiniCalc& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
