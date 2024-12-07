#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEFAULT_DEVICE "/dev/otpdev0"

#define OTP_IOC_MAGIC 'k'
#define OTP_IOC_ADD _IOW(OTP_IOC_MAGIC, 1, char *)
#define OTP_IOC_DEL _IOW(OTP_IOC_MAGIC, 2, char *)
#define OTP_IOC_LIST _IOR(OTP_IOC_MAGIC, 3, char *)

void add_password(int fd, const char *password) {
    if (ioctl(fd, OTP_IOC_ADD, password) < 0)
        perror("Erreur ajout mot de passe");
    else
        printf("Mot de passe ajouté : %s\n", password);
}

void del_password(int fd, const char *password) {
    if (ioctl(fd, OTP_IOC_DEL, password) < 0)
        perror("Erreur suppression mot de passe");
    else
        printf("Mot de passe supprimé : %s\n", password);
}

void list_passwords(int fd) {
    char buffer[1024] = {0};
    if (ioctl(fd, OTP_IOC_LIST, buffer) < 0)
        perror("Erreur liste mots de passe");
    else
        printf("Liste des mots de passe OTP :\n%s", buffer);
}

void generate_otp(int fd) {
    char buffer[64] = {0};
    ssize_t ret = read(fd, buffer, sizeof(buffer) - 1);
    if (ret < 0)
        perror("Erreur génération OTP");
    else {
        buffer[ret] = '\0';
        printf("OTP généré : %s", buffer);
    }
}

void print_usage(const char *prog_name) {
    printf("Utilisation : %s <device> <commande> [<arguments>]\n", prog_name);
    printf("Périphérique par défaut : %s\n", DEFAULT_DEVICE);
    printf("Commandes : add <mot_de_passe>, del <mot_de_passe>, list, get\n");
    printf("Exemple : %s /dev/otpdev1 add monmotdepasse\n", prog_name);
}

int main(int argc, char *argv[]) {
    int fd;
    const char *device = DEFAULT_DEVICE;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Vérifie si le premier argument est un périphérique
    if (strncmp(argv[1], "/dev/otpdev", 10) == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        device = argv[1];
    }

    // Détermine l'index de la commande
    int cmd_index = (strncmp(argv[1], "/dev/otpdev", 10) == 0) ? 2 : 1;

    if (argc <= cmd_index) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Erreur ouverture périphérique");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[cmd_index], "add") == 0 && argc == cmd_index + 2)
        add_password(fd, argv[cmd_index + 1]);
    else if (strcmp(argv[cmd_index], "del") == 0 && argc == cmd_index + 2)
        del_password(fd, argv[cmd_index + 1]);
    else if (strcmp(argv[cmd_index], "list") == 0 && argc == cmd_index + 1)
        list_passwords(fd);
    else if (strcmp(argv[cmd_index], "get") == 0 && argc == cmd_index + 1)
        generate_otp(fd);
    else
        print_usage(argv[0]);

    close(fd);
    return EXIT_SUCCESS;
}
