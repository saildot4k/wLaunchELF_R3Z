#ifndef KELF_SIGNER_H
#define KELF_SIGNER_H

#define KELF_SIGN_HEADER_FMCB 0
#define KELF_SIGN_HEADER_DNASLOAD 1
#define KELF_SIGN_HEADER_DONGLE 2

#define KELF_SIGN_SYSTEM_PS2 0
#define KELF_SIGN_SYSTEM_PSX 1

int KelfSignGetOutputSize(const char *elf_path, int *out_size);
int KelfSignValidateKeys(const char *keys_path, const char *keyset);
int KelfSignElfToMemory(const char *elf_path,
                        const char *keys_path,
                        const char *keyset,
                        int header_id,
                        int system_type,
                        void **out_buf,
                        int *out_size);

#endif
