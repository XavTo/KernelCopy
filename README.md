---

## `README.md` (Documentation Globale)

# Documentation du Projet OTP

Ce projet illustre deux méthodes différentes pour gérer des One-Time Passwords (OTP) via des modules kernel Linux et des utilitaires en espace utilisateur. Chaque méthode crée un ou plusieurs périphériques caractères dans `/dev/` et fournit un utilitaire pour interagir avec ces périphériques.

## Prérequis

- Système Linux avec support pour la compilation de modules kernel (headers du noyau installés).
- Droits administrateur (`sudo`) pour charger et décharger les modules.
- Outils de compilation (`gcc`, `make`).

  ### Transférer le code vers une VM via SSH

  1. Connectez-vous en SSH à votre VM pour vérifier l'accès :

     ```bash
     ip a
     ```

     ```bash
     ssh $(whoami)@monIp
     ```

  2. Utilisez la commande suivante pour transférer le répertoire du projet vers votre VM :

     ```bash
     scp -r $(pwd) $(whoami)@monIp:/home/$(whoami)/otp_list/
     ```

     Remplacez `monIp` par l'adresse IP de votre VM, si différente.

  3. Une fois le transfert terminé, connectez-vous à votre VM et accédez au projet transféré :

     ```bash
     ssh $(whoami)@monIp
     cd /home/$(whoami)/otp_list/
     ```

## Compilation Générale

Le projet est divisé en deux répertoires :

- `src/` : Contient le code des modules kernel.
- `utils/` : Contient les utilitaires en espace utilisateur.

### Compiler les Modules Kernel

1. Accédez au répertoire `src/` :

   ```bash
   cd src
   ```

2. Compilez les modules :

   ```bash
   make
   ```

   Cela génère les modules `otp_list.ko` et `otp_time.ko`.

3. Pour nettoyer les fichiers compilés :

   ```bash
   make clean
   ```

### Compiler les Utilitaires

1. Accédez au répertoire `utils/` :

   ```bash
   cd ../utils
   ```

2. Compilez les utilitaires :

   ```bash
   make
   ```

   Cela génère les exécutables `otp_test` et `timeotp_test`.

3. Pour nettoyer les exécutables compilés :

   ```bash
   make clean
   ```

### Chargement du Module

1. Chargez le module kernel :

   ```bash
   cd ../src
   ```

   ```bash
   sudo insmod otp_list.ko
   ```

2. Vérifiez la création des périphériques :

   ```bash
   ls /dev/otpdev*
   ```

   Vous devriez voir `/dev/otpdev0` à `/dev/otpdev4` (pour `MAX_DEVICES=5`).

## Utilisation des Modules et Utilitaires

### 1. Première Méthode : Liste de Mots de Passe (`otp_list`)

#### Utilisation de l'Utilitaire `otp_test`

Consultez la documentation détaillée dans [`utils/README.md`](utils/README.md).

##### Exemples de Commandes

- **Aller dans le dossier utils**

  ```bash
   cd ../utils
  ```

- **Ajouter un mot de passe** :

  ```bash
  ./otp_test add secret
  ```

  ```bash
  ./otp_test add secret2
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
  ./otp_test del secret
  ```

- **Utiliser un autre périphérique** :

  ```bash
  ./otp_test /dev/otpdev1 add autrepass
  ./otp_test /dev/otpdev1 list
  ```

- **Voir les logs kernel** :

  ```bash
  sudo dmesg | tail -n 20
  ```

#### Déchargement du Module

1. Déchargez le module kernel :

   ```bash
   sudo rmmod otp_list
   ```

2. Vérifiez la suppression des périphériques :

   ```bash
   ls /dev/otpdev*
   ```

### 2. Deuxième Méthode : Clé et Temps (TOTP) (`otp_time`)

#### Chargement du Module

1. Chargez le module kernel :

   ```bash
   sudo insmod otp_time.ko
   ```

2. Vérifiez la création du périphérique :

   ```bash
   ls /dev/timeotp0
   ```

#### Utilisation de l'Utilitaire `timeotp_test`

Consultez la documentation détaillée dans [`utils/README.md`](utils/README.md).

##### Exemples de Commandes

- **Définir la clé secrète** :

  ```bash
  ./timeotp_test set_key mysecretkey
  ```

- **Définir la durée de validité** :

  ```bash
  ./timeotp_test set_duration 60
  ```

- **Générer un OTP** :

  ```bash
  ./timeotp_test get
  ```

- **Changer la clé ou la durée** :

  ```bash
  ./timeotp_test set_key anotherkey
  ./timeotp_test set_duration 45
  ```

#### Déchargement du Module

1. Déchargez le module kernel :

   ```bash
   sudo rmmod otp_time
   ```

2. Vérifiez la suppression du périphérique :

   ```bash
   ls /dev/timeotp0
   ```

## Conseils

- **Permissions** : Les périphériques sont créés avec des permissions `0666`, permettant un accès en lecture et écriture à tous les utilisateurs. Cela facilite les tests sans nécessiter de `sudo` pour les utilitaires.

- **Conflits de Noms** : Si vous rencontrez des erreurs lors de la création des périphériques (comme `EEXIST`), assurez-vous qu'aucun autre module ou périphérique avec le même nom n'est chargé. Redémarrer le système peut aider à nettoyer les entrées résiduelles dans `sysfs`.

- **Sécurité** : Les exemples fournis utilisent des permissions `0666` pour simplifier les tests. En production, ajustez ces permissions pour restreindre l'accès aux périphériques selon vos besoins de sécurité.

## Nettoyage Complet

Pour nettoyer tous les modules et utilitaires compilés :

1. Nettoyez les modules kernel :

   ```bash
   cd src
   make clean
   ```

2. Nettoyez les utilitaires :

   ```bash
   cd ../utils
   make clean
   ```

## Conclusion

Vous disposez désormais de deux solutions complètes pour gérer des OTP via des modules kernel Linux :

1. **Liste de Mots de Passe**

   - **Module** : `otp_list.ko`
   - **Utilitaire** : `otp_test`  
     Permet d'ajouter, supprimer, lister et générer des OTP depuis une liste statique dans le noyau.

2. **Clé et Temps (TOTP RFC 6238)**
   - **Module** : `otp_time.ko`
   - **Utilitaire** : `timeotp_test`  
     Permet de définir une clé, une durée de validité, et de générer des OTP en fonction du temps courant.

Ces deux méthodes répondent aux objectifs du projet en mettant en œuvre deux approches différentes pour la génération et la gestion des OTP, avec une interface utilisateur claire et des périphériques faciles à manipuler.
