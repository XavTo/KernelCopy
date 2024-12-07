#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEFAULT_DEVICE "/dev/timeotp0"

#define OTP_IOC_MAGIC 'k'
#define OTP_IOC_SET_KEY _IOW(OTP_IOC_MAGIC, 1, char *)
#define OTP_IOC_SET_DURATION _IOW(OTP_IOC_MAGIC, 2, int)

void set_key(int fd, const char *key) {
    char buffer[64];
    size_t key_len = strlen(key);

    if (key_len >= sizeof(buffer)) {
        fprintf(stderr, "Erreur : La clé ne doit pas dépasser %zu caractères.\n", sizeof(buffer)-1);
        return;
    }

    strncpy(buffer, key, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    if (ioctl(fd, OTP_IOC_SET_KEY, buffer) < 0) {
        perror("Erreur définition clé");
    } else {
        printf("Clé définie : %s\n", buffer);
    }
}

void set_duration(int fd, int duration) {
    if (duration <= 0) {
        fprintf(stderr, "Durée invalide (%d). Utilisation de la valeur par défaut : 30 secondes.\n", duration);
        duration = 30;
    }

    if (ioctl(fd, OTP_IOC_SET_DURATION, &duration) < 0) {
        perror("Erreur définition durée");
    } else {
        printf("Durée définie à : %d secondes\n", duration);
    }
}

void generate_otp(int fd) {
    char buffer[64];
    ssize_t ret = read(fd, buffer, sizeof(buffer)-1);
    if (ret < 0) {
        perror("Erreur génération OTP");
    } else if (ret == 0) {
        fprintf(stderr, "Aucun OTP généré.\n");
    } else {
        buffer[ret] = '\0';
        printf("OTP généré : %s", buffer);
    }
}

void print_usage(const char *prog_name) {
    printf("Utilisation : %s <commande> [arguments]\n", prog_name);
    printf("Sans arguments, device par défaut : %s\n", DEFAULT_DEVICE);
    printf("Commandes :\n");
    printf("  set_key <clé>\n");
    printf("  set_duration <secondes>\n");
    printf("  get\n");
    printf("Exemples :\n");
    printf("  %s set_key mysecretkey\n", prog_name);
    printf("  %s set_duration 60\n", prog_name);
    printf("  %s get\n", prog_name);
}

int main(int argc, char *argv[]) {
    int fd;
    const char *device = DEFAULT_DEVICE;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Erreur ouverture périphérique");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "set_key") == 0) {
        if (argc == 3) {
            set_key(fd, argv[2]);
        } else {
            fprintf(stderr, "Erreur : 'set_key' nécessite un argument <clé>.\n");
            print_usage(argv[0]);
        }
    }
    else if (strcmp(argv[1], "set_duration") == 0) {
        if (argc == 3) {
            int d = atoi(argv[2]);
            set_duration(fd, d);
        } else {
            fprintf(stderr, "Erreur : 'set_duration' nécessite un argument <secondes>.\n");
            print_usage(argv[0]);
        }
    }
    else if (strcmp(argv[1], "get") == 0) {
        if (argc == 2) {
            generate_otp(fd);
        } else {
            fprintf(stderr, "Erreur : 'get' ne nécessite aucun argument supplémentaire.\n");
            print_usage(argv[0]);
        }
    }
    else {
        fprintf(stderr, "Commande inconnue : %s\n", argv[1]);
        print_usage(argv[0]);
    }

    close(fd);
    return EXIT_SUCCESS;
}
