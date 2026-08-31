#ifndef COALESCE_SSH_H
#define COALESCE_SSH_H

#include <stdint.h>
#include <stddef.h>

#define PROTOVERSION   "2.0"
#define SOFTWARE_VER   "coalesce_0.0.1"
#define COMMENTS       ""
#define IDENT_STRING   "SSH-" PROTOVERSION "-" SOFTWARE_VER COMMENTS "\r\n"

/* Maximum packet size per RFC 4253 (must support at least 32768 byte payload + 35000 total) */
#define MAX_PACKET_SIZE     35000
#define MAX_PAYLOAD_SIZE    32768
#define MIN_PADDING_SIZE    4

/* ── Transport Layer Message Numbers (RFC 4250 & RFC 4253) ── */
#define SSH_MSG_DISCONNECT                1
#define SSH_MSG_IGNORE                    2
#define SSH_MSG_UNIMPLEMENTED             3
#define SSH_MSG_DEBUG                     4
#define SSH_MSG_SERVICE_REQUEST           5
#define SSH_MSG_SERVICE_ACCEPT            6
#define SSH_MSG_KEXINIT                   20
#define SSH_MSG_NEWKEYS                   21

/* ── Key Exchange Specific Messages ── */
#define SSH_MSG_KEXDH_INIT                30
#define SSH_MSG_KEXDH_REPLY               31
#define SSH_MSG_KEX_ECDH_INIT             30
#define SSH_MSG_KEX_ECDH_REPLY            31

/* ── User Authentication Protocol Messages (RFC 4252) ── */
#define SSH_MSG_USERAUTH_REQUEST          50
#define SSH_MSG_USERAUTH_FAILURE          51
#define SSH_MSG_USERAUTH_SUCCESS          52
#define SSH_MSG_USERAUTH_BANNER           53
#define SSH_MSG_USERAUTH_PK_OK            60
#define SSH_MSG_USERAUTH_PW_CHANGEREQ     60
#define SSH_MSG_USERAUTH_INFO_REQUEST     60
#define SSH_MSG_USERAUTH_INFO_RESPONSE    61

/* ── Connection Protocol Messages (RFC 4254) ── */
#define SSH_MSG_GLOBAL_REQUEST            80
#define SSH_MSG_REQUEST_SUCCESS           81
#define SSH_MSG_REQUEST_FAILURE           82
#define SSH_MSG_CHANNEL_OPEN              90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_OPEN_FAILURE      92
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     93
#define SSH_MSG_CHANNEL_DATA              94
#define SSH_MSG_CHANNEL_EXTENDED_DATA     95
#define SSH_MSG_CHANNEL_EOF               96
#define SSH_MSG_CHANNEL_CLOSE             97
#define SSH_MSG_CHANNEL_REQUEST           98
#define SSH_MSG_CHANNEL_SUCCESS           99
#define SSH_MSG_CHANNEL_FAILURE           100

/* ── Disconnect Reason Codes (RFC 4250 §4.2.2) ── */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT      1
#define SSH_DISCONNECT_PROTOCOL_ERROR                   2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED              3
#define SSH_DISCONNECT_RESERVED                         4
#define SSH_DISCONNECT_MAC_ERROR                        5
#define SSH_DISCONNECT_COMPRESSION_ERROR                6
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE            7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED   8
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE          9
#define SSH_DISCONNECT_CONNECTION_LOST                  10
#define SSH_DISCONNECT_BY_APPLICATION                   11
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS             12
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER           13
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE   14
#define SSH_DISCONNECT_ILLEGAL_USER_NAME                15

/* ── Channel Open Failure Codes (RFC 4250 §4.2.3) ── */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED            1
#define SSH_OPEN_CONNECT_FAILED                         2
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE                   3
#define SSH_OPEN_RESOURCE_SHORTAGE                      4

/* ── Extended Data Type Codes ── */
#define SSH_EXTENDED_DATA_STDERR                        1


/*compressions to be supported*/
/*
    none     REQUIRED        no compression
    zlib     OPTIONAL        ZLIB (LZ77) compression
*/

/*Encryption*/
/*
      3des-cbc         REQUIRED          three-key 3DES in CBC mode
      blowfish-cbc     OPTIONAL          Blowfish in CBC mode
      twofish256-cbc   OPTIONAL          Twofish in CBC mode,
                                         with a 256-bit key
      twofish-cbc      OPTIONAL          alias for "twofish256-cbc"
                                         (this is being retained
                                         for historical reasons)
      twofish192-cbc   OPTIONAL          Twofish with a 192-bit key
      twofish128-cbc   OPTIONAL          Twofish with a 128-bit key
      aes256-cbc       OPTIONAL          AES in CBC mode,
                                         with a 256-bit key
      aes192-cbc       OPTIONAL          AES with a 192-bit key
      aes128-cbc       RECOMMENDED       AES with a 128-bit key
      serpent256-cbc   OPTIONAL          Serpent in CBC mode, with
                                         a 256-bit key
      serpent192-cbc   OPTIONAL          Serpent with a 192-bit key
      serpent128-cbc   OPTIONAL          Serpent with a 128-bit key
      arcfour          OPTIONAL          the ARCFOUR stream cipher
                                         with a 128-bit key
      idea-cbc         OPTIONAL          IDEA in CBC mode
      cast128-cbc      OPTIONAL          CAST-128 in CBC mode
      none             OPTIONAL          no encryption; NOT RECOMMENDED
*/

/*Data Integrity*/
/*
      hmac-sha1    REQUIRED        HMAC-SHA1 (digest length = key
                                   length = 20)
      hmac-sha1-96 RECOMMENDED     first 96 bits of HMAC-SHA1 (digest
                                   length = 12, key length = 20)
      hmac-md5     OPTIONAL        HMAC-MD5 (digest length = key
                                   length = 16)
      hmac-md5-96  OPTIONAL        first 96 bits of HMAC-MD5 (digest
                                   length = 12, key length = 16)
      none         OPTIONAL        no MAC; NOT RECOMMENDED
*/

/*Key Exchange Methods*/
/*
      diffie-hellman-group1-sha1 REQUIRED
      diffie-hellman-group14-sha1 REQUIRED
*/

/*Public Key Algorithms*/
/*
   ssh-dss           REQUIRED     sign   Raw DSS Key
   ssh-rsa           RECOMMENDED  sign   Raw RSA Key
   pgp-sign-rsa      OPTIONAL     sign   OpenPGP certificates (RSA key)
   pgp-sign-dss      OPTIONAL     sign   OpenPGP certificates (DSS key)
*/
#endif


