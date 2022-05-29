#define VERSION _IOR('N', 0, struct parameters)
#define REWIND _IOR('N', 1, struct parameters_rewind)
#define SIZE _IOR('N', 2, int)

struct parameters {
    int size;
    int version;
    char cible[0];
};

struct parameters_rewind {
    int size;
    char cible[0];
};