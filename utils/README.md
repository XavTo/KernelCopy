### `utils/README.md`

# Utilitaires `otp_test` et `timeotp_test`

## Description

Ce document détaille les utilitaires en espace utilisateur permettant d'interagir avec les modules kernel OTP. Deux utilitaires sont fournis :

- **`otp_test`** : Interagit avec le module OTP basé sur une liste de mots de passe.
- **`timeotp_test`** : Interagit avec le module OTP basé sur une clé secrète et le temps (TOTP simplifié).

## Compilation des Utilitaires

Assurez-vous d'être dans le répertoire `utils/` puis exécutez :

```bash
make
```

Cela génère les exécutables `otp_test` et `timeotp_test`.

## Utilisation de `otp_test`

`otp_test` permet d'ajouter, de supprimer, de lister et de générer des mots de passe OTP à partir d'une liste statique.

### Commandes Disponibles

- **Ajouter un mot de passe** :

  ```bash
  ./otp_test add <mot_de_passe>
  ```

- **Lister les mots de passe** :

  ```bash
  ./otp_test list
  ```

- **Générer un OTP** :

  ```bash
  ./otp_test get
  ```

- **Supprimer un mot de passe** :

  ```bash
  ./otp_test del <mot_de_passe>
  ```

- **Utiliser un périphérique spécifique** :
  ```bash
  ./otp_test /dev/otpdev1 add <mot_de_passe>
  ./otp_test /dev/otpdev1 list
  ```

### Exemple d'Utilisation

```bash
./otp_test add secret1
./otp_test list
./otp_test get
./otp_test del secret1
```

## Utilisation de `timeotp_test`

`timeotp_test` permet de configurer une clé secrète et une durée de validité pour générer des OTP basés sur le temps.

### Commandes Disponibles

- **Définir une clé secrète** :

  ```bash
  ./timeotp_test set_key <clé_secrète>
  ```

- **Définir une durée de validité (en secondes)** :

  ```bash
  ./timeotp_test set_duration <durée_en_secondes>
  ```

- **Générer un OTP** :

  ```bash
  ./timeotp_test get
  ```

### Exemple d'Utilisation

```bash
./timeotp_test set_key mysecretkey
./timeotp_test set_duration 60
./timeotp_test get
```

## Remarques Importantes

- Assurez-vous que les modules kernel (`otp_list.ko` ou `otp_time.ko`) sont chargés avant d'utiliser les utilitaires.
- Les périphériques doivent être visibles dans `/dev/`, par exemple `/dev/otpdev0` ou `/dev/timeotp0`.
- Utilisez `sudo` si nécessaire pour charger/décharger les modules ou pour accéder aux périphériques.
