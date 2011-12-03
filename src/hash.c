int DJB_str_hash(char* str) {
    int hash = 5381;
    while(*str) {
        hash = ((hash << 5) + hash) + (*str);
        str++;
    }
    return hash;
}
