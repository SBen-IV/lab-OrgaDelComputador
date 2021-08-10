#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

enum encoding {
    UTF8, UTF16BE, UTF16LE, UTF32BE, UTF32LE,
};

/*
 * Devuelve el encoding correspondiente al argumento.
 */
static enum encoding str_to_encoding(const char *enc) {
    if (strcmp(enc, "UTF-8") == 0)
        return UTF8;
    else if (strcmp(enc, "UTF-16BE") == 0)
        return UTF16BE;
    else if (strcmp(enc, "UTF-16LE") == 0)
        return UTF16LE;
    else if (strcmp(enc, "UTF-32BE") == 0)
        return UTF32BE;
    else if (strcmp(enc, "UTF-32LE") == 0)
        return UTF32LE;
    else
        return -1;
}

/*
 * Devuelve el encoding correspondiente al byte order mark (BOM).
 */
static enum encoding bom_to_encoding(uint8_t *bom) {
    if(bom[0] == 0xFE && bom[1] == 0xFF) {
        return UTF16BE;
    }
    if(bom[0] == 0x00 && bom[1] == 0x00 && bom[2] == 0xFE && bom[3] == 0xFF){
         return UTF32BE;
    }
    if(bom[0] == 0xFF && bom[1] == 0xFE && bom[2] == 0x00 && bom[3] == 0x00){
        return UTF32LE;
    }
    if(bom[0] == 0xFF && bom[1] == 0xFE){
        return UTF16LE;
    }

    // Si no es BOM válida, se asume UTF-8.
    return UTF8;
}

void utf8_to_ucs4(uint8_t* buf, int* b, uint32_t* codepoint){

    if((buf[(*b)] & 0x80) == 0){
        (*codepoint) = buf[(*b)++];
    }else if((buf[(*b)] & 0x20) == 0){
        (*codepoint) = buf[(*b)++] & 0x1F;
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
    }else if((buf[(*b)] & 0x10) == 0){
        (*codepoint) = buf[(*b)++] & 0xF;
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
    }else{
        (*codepoint) = buf[(*b)++] & 0x7;
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
        (*codepoint) = ((*codepoint) << 6) | (buf[(*b)++] & 0x3F);
    }
}

void utf16le_to_ucs4(uint8_t* buf, int* b, uint32_t* codepoint){

    if(((buf[(*b)+1] & 0xD8) == 0xD8) && ((buf[(*b)+3] & 0xDC) == 0xDC)){
        uint32_t high_surrogate = buf[(*b)+1];
        high_surrogate = ((high_surrogate << 8) | buf[(*b)]) - 0xD800; (*b) += 2;

        uint32_t low_surrogate = buf[(*b)+1];
        low_surrogate = ((low_surrogate << 8) | buf[(*b)]) - 0xDC00; (*b) += 2;

        (*codepoint) = ((high_surrogate << 10) | low_surrogate) + 0x10000;
    }else{
        (*codepoint) = buf[(*b)+1];
        (*codepoint) = ((*codepoint) << 8) | buf[(*b)];
        (*b) += 2;
    }
}

void utf16be_to_ucs4(uint8_t* buf, int* b, uint32_t* codepoint){

    if(((buf[(*b)] & 0xD8) == 0xD8) && ((buf[(*b)+2] & 0xDC) == 0xDC)){
        uint32_t high_surrogate = buf[(*b)++];
        high_surrogate = ((high_surrogate << 8) | buf[(*b)++]) - 0xD800;

        uint32_t low_surrogate = buf[(*b)++];
        low_surrogate = ((low_surrogate << 8) | buf[(*b)++]) - 0xDC00;

        (*codepoint) = ((high_surrogate << 10) | low_surrogate) + 0x10000;
    }else{
        (*codepoint) = buf[(*b)++];
        (*codepoint) = ((*codepoint) << 8) | buf[(*b)++];
    }
}

/*
 * Devuelve verdadero si hay un codepoint codificado en buf.
 */
static bool has_codepoint(enum encoding enc, uint8_t *buf, int n) {
    switch (enc) {
        case UTF32BE:
        case UTF32LE:
            return n >= 4;
        case UTF16BE:
            return (n >= 4 ||
                (n >= 2 && (buf[0] & 0xD8) == 0));
        case UTF16LE:
            return (n >= 4 ||
                (n >= 2 && (buf[1] & 0xD8) == 0));
        case UTF8:
            return (n >= 4 ||
                (n >= 3 && buf[0] <= 0xEF) ||
                (n >= 2 && buf[0] <= 0xDF) ||
                (n >= 1 && buf[0] <= 0x7F));
        default:
            return false;
    }
}

/*
 * Transforma una codificación a UCS-4.
 *
 * Argumentos:
 *
 *   - enc: el encoding original.
 *   - buf: los bytes originales.
 *   - nbytes: número de bytes disponibles en buf.
 *   - destbuf: resultado de la conversión.
 *
 * Devuelve: el número de CODEPOINTS obtenidos (número
 *           de elementos escritos en destbuf).
 *
 * Actualiza: nbytes contiene el número de bytes que no se
 *            pudieron consumir (secuencia o surrgate incompleto).
 *
 * Se debe garantiza que "destbuf" puede albergar al menos nbytes
 * elementos (caso enc=UTF-8, buf=ASCII).
 */
int orig_to_ucs4(enum encoding enc, uint8_t *buf, int* nbytes, uint32_t *destbuf) {

    int cant_cp = 0, b = 0;

    while(has_codepoint(enc, &buf[b], *nbytes) && b < (*nbytes)){
        uint32_t codepoint = 0;

        switch(enc){
            case UTF32BE:
                codepoint = codepoint | buf[b++];
                codepoint = (codepoint << 8) | buf[b++];
                codepoint = (codepoint << 8) | buf[b++];
                codepoint = (codepoint << 8) | buf[b++];
                break;
            case UTF32LE:
                codepoint = codepoint | buf[b+3];
                codepoint = ((codepoint << 8) | buf[b+2]);
                codepoint = ((codepoint << 8) | buf[b+1]);
                codepoint = ((codepoint << 8) | buf[b]);
                b += 4;
                break;
            case UTF16LE:
                utf16le_to_ucs4(buf, &b, &codepoint);
                break;
            case UTF16BE:
                utf16be_to_ucs4(buf, &b, &codepoint);
                break;
            case UTF8:
                utf8_to_ucs4(buf, &b, &codepoint);
                break;
        }

        destbuf[cant_cp++] = codepoint;
    }

    (*nbytes) = (*nbytes) - b;

    return cant_cp;
}

void ucs4_to_utf8(uint32_t input, uint8_t* outbuf, int* b){

    uint32_t input_be = input;

    if((input_be <= 0x7F)){
        outbuf[(*b)++] = input;
    }else if(input_be <= 0x7FF){
        outbuf[(*b)++] = 0xC0 | (input >> 6);
        outbuf[(*b)++] = 0x80 | (input & 0x3F);
    }else if(input_be <= 0xFFFF){
        /*
           Not
           shown */
    }else{
        /*
           Not
           shown
                 */
    }   
}

void ucs4_to_utf16le(uint32_t input, uint8_t* outbuf, int* b){

    if((input > 0xFFFF) == 0){
        outbuf[(*b)++] = input & 0xFF;
        outbuf[(*b)++] = (input >> 8);
    }else{
        uint32_t aux = input;
        aux = aux - 0x10000;
        
        uint32_t high_surrogate = ((aux >> 10) + 0xD800);

        outbuf[(*b)++] = high_surrogate & 0xFF;
        outbuf[(*b)++] = high_surrogate >> 8;

        uint32_t low_surrogate = (aux & 0x3FF) + 0xDC00;

        outbuf[(*b)++] = low_surrogate & 0xFF;
        outbuf[(*b)++] = low_surrogate >> 8;
    }
}

void ucs4_to_utf16be(uint32_t input, uint8_t* outbuf, int* b){

    if((input > 0xFFFF) == 0){
        outbuf[(*b)++] = (input >> 8);
        outbuf[(*b)++] = (input & 0xFF);
    }else{
        uint32_t aux = input;
        aux = aux - 0x10000;
        
        uint32_t high_surrogate = ((aux >> 10) + 0xD800);

        outbuf[(*b)++] = high_surrogate >> 8;
        outbuf[(*b)++] = high_surrogate & 0xFF;

        uint32_t low_surrogate = (aux & 0x3FF) + 0xDC00;

        outbuf[(*b)++] = low_surrogate >> 8;
        outbuf[(*b)++] = low_surrogate & 0xFF;
    }
}

/*
 * Transforma UCS-4 a la codificación deseada.
 *
 * Argumentos:
 *
 *   - enc: el encoding destino.
 *   - input: los codepoints a codificar.
 *   - npoints: el número de codepoints en input.
 *   - output: resultado de la conversión.
 *
 * Devuelve: el número de BYTES obtenidos (número
 *           de elementos escritos en destbuf).
 *
 * Se debe garantiza que "destbuf" puede albergar al menos npoints*4
 * elementos, o bytes (caso enc=UTF-32).
 */
int ucs4_to_dest(enum encoding enc, uint32_t *input, int npoints, uint8_t *outbuf) {
    
    int b = 0;

    for (int i = 0; i < npoints; i++) {
        switch (enc) {
            case UTF32BE:
                outbuf[b++] = input[i] >> 24;
                outbuf[b++] = (input[i] >> 16) & 0xFF;
                outbuf[b++] = (input[i] >> 8) & 0xFF;
                outbuf[b++] = input[i] & 0xFF;
                break;
            case UTF32LE:
                outbuf[b++] = input[i] & 0xFF;
                outbuf[b++] = (input[i] >> 8) & 0xFF;
                outbuf[b++] = (input[i] >> 16) & 0xFF;
                outbuf[b++] = input[i] >> 24;
                break;
            case UTF16LE:
                ucs4_to_utf16le(input[i], outbuf, &b);
                break;
            case UTF16BE:
                ucs4_to_utf16be(input[i], outbuf, &b);
                break;
            case UTF8:
                ucs4_to_utf8(input[i], outbuf, &b);
                break;
        }
    }

    return b;
}

void add_first_dest_bom(enum encoding dest_enc, uint8_t* outbuf, int *outbytes){

    uint32_t outbuf_aux[(*outbytes)];
    memcpy(outbuf_aux, outbuf, *outbytes);

    switch(dest_enc){
        case UTF8:
            break;
        case UTF16LE:
            outbuf[0] = 0xFF;
            outbuf[1] = 0xFE;
            memcpy(outbuf+2, outbuf_aux, *outbytes);
            (*outbytes) += 2;
            break;
        case UTF16BE:
            outbuf[0] = 0xFE;
            outbuf[1] = 0xFF;
            memcpy(outbuf+2, outbuf_aux, *outbytes);
            (*outbytes) += 2;
            break;
        case UTF32LE:
            outbuf[0] = 0xFF;
            outbuf[1] = 0xFE;
            outbuf[2] = 0x00;
            outbuf[3] = 0x00;
            memcpy(outbuf+4, outbuf_aux, *outbytes);
            (*outbytes) += 4;
            break;
        case UTF32BE:
            outbuf[0] = 0x00;
            outbuf[1] = 0x00;
            outbuf[2] = 0xFE;
            outbuf[3] = 0xFF;
            memcpy(outbuf+4, outbuf_aux, *outbytes);
            (*outbytes) += 4;
            break;
    }
}

int main(int argc, char *argv[]) {
    enum encoding orig_enc, dest_enc;

    if(argc != 2) {
        fprintf(stderr, "Uso: ./recode57 <encoding>\n");
        return 1;
    }

    if((dest_enc = str_to_encoding(argv[1])) > 4) {
        fprintf(stderr, "Encoding no válido: %s\n", argv[1]);
        return 1;
    }

    // Detectar codificación origen con byte order mark.
    uint8_t bom[4];

    if(read(STDIN_FILENO, bom, 4) < 0){
        perror("Error");
        return -1;
    }

    orig_enc = bom_to_encoding(bom);

    // En cada iteración, leer hasta 1024 bytes, convertirlos a UCS-4
    // (equivalente a UTF32-LE con enteros nativos) y convertirlo por
    // salida estándar.
    uint8_t inbuf[1024];
    uint8_t outbuf[1024*4];
    uint32_t ucs4[1024];
    ssize_t inbytes;
    int npoints, outbytes, prevbytes = 0;

    // Si orig_enc no fue UTF-32, quedaron 2 o 4 bytes en "bom" que
    // deben ser prefijados en inbuf.
    if (orig_enc == UTF8) {
        memcpy(inbuf, bom, 4);
        prevbytes = 4;
    } else if (orig_enc == UTF16BE || orig_enc == UTF16LE) {
        memcpy(inbuf, bom + 2, 2);
        prevbytes = 2;
    }

    bool first_dest_bom = true;

    while((inbytes = read(STDIN_FILENO, inbuf + prevbytes, sizeof(inbuf) - prevbytes)) > 0) {
        prevbytes += inbytes;

        npoints = orig_to_ucs4(orig_enc, inbuf, &prevbytes, ucs4);
        outbytes = ucs4_to_dest(dest_enc, ucs4, npoints, outbuf);

        if(first_dest_bom){
            add_first_dest_bom(dest_enc, outbuf, &outbytes);
            first_dest_bom = false;
        }

        ssize_t written_bytes = 0;
        
        do{
           written_bytes += write(STDOUT_FILENO, outbuf + written_bytes, outbytes - written_bytes);
        }while(written_bytes < outbytes);

        if (prevbytes > 0) {
            uint8_t inbuf_aux[1024];
            memcpy(inbuf_aux, inbuf+(inbytes-prevbytes), prevbytes);
            memcpy(inbuf, inbuf_aux, prevbytes);
        }
    }

    return 0;
}
