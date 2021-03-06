#include <Windows.h>
#include <ImageHlp.h>

#include <openssl/x509.h>

#include "log.h"


int main( int argc, char **argv, char **env )
{
    CHAR* FileName;
    HANDLE hFile;
    LPWIN_CERTIFICATE Certificate;
    DWORD CertificateLength;
    BOOL Result;

    unsigned char *CertData;
    long CertDataLength;

    BIO *Out;

    PKCS7 *PKCS7Object;
    int Nid, CertIndex;
    STACK_OF(X509) *X509Certs;
    STACK_OF(X509_CRL) *X509Crls;
    X509 *X509Cert;
    X509_NAME *X509IssuerName;
    X509_NAME *X509SubjectName;
    const ASN1_INTEGER *SerialNumber;
    X509_PUBKEY *X509PublicKey;
    const unsigned char *PublicKeyData;
    int PublicKeyLength;
    X509_ALGOR *X509Algorithm;
    const ASN1_OBJECT *AlgorithmType;
    RSA *PublicKey;
    const BIGNUM *ModulusNum, *ExponentNum;
    ASN1_INTEGER *Modulus;


    if (argc < 2)
    {
        FileName = strrchr( argv[0], '\\' );
        if (!FileName)
            FileName = strrchr( argv[0], '/' );

        printf( "Usage:\n"
                "  %s PATH", FileName + 1 );

        return -1;
    }
    
    hFile = CreateFileA( argv[1],
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        LogError( "CreateFile failed with error 0x%08X", GetLastError( ) );
        return -1;
    }

    CertificateLength = 0;
    Result = ImageGetCertificateData( hFile, 0, NULL, &CertificateLength );
    
    Certificate = (LPWIN_CERTIFICATE)HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, CertificateLength );
    if (Certificate)
    {
        Result = ImageGetCertificateData( hFile, 0, Certificate, &CertificateLength );
        if (Result)
        {

            CertData = (unsigned char*)Certificate->bCertificate;
            CertDataLength = CertificateLength - FIELD_OFFSET( WIN_CERTIFICATE, bCertificate );

            Out = BIO_open_default( 'w' );

            //LogDumpBuffer( (uint8_t*)CertData, CertDataLength, FALSE );

            PKCS7Object = d2i_PKCS7( NULL, (const unsigned char**)&CertData, CertDataLength );
            if (PKCS7Object != NULL)
            {
                Nid = OBJ_obj2nid( PKCS7Object->type );
                if (Nid == NID_pkcs7_signed)
                {
                    if (PKCS7Object->d.sign != NULL)
                    {
                        X509Certs = PKCS7Object->d.sign->cert;
                        X509Crls = PKCS7Object->d.sign->crl;
                    }
                }
                else if (Nid == NID_pkcs7_signedAndEnveloped)
                {
                    if (PKCS7Object->d.signed_and_enveloped != NULL)
                    {
                        X509Certs = PKCS7Object->d.signed_and_enveloped->cert;
                        X509Crls = PKCS7Object->d.signed_and_enveloped->crl;
                    }
                }
                else
                {
                    X509Certs = NULL;
                    X509Crls = NULL;
                }

                if (X509Certs != NULL)
                {
                    for (CertIndex = 0; CertIndex < sk_X509_num( X509Certs ); CertIndex++)
                    {
                        X509Cert = sk_X509_value( X509Certs, CertIndex );

                        //X509_print_ex( Out, X509Cert, XN_FLAG_MULTILINE, X509_FLAG_COMPAT );

                        BIO_printf( Out, "Certificate (%d)\n", CertIndex );

                        //
                        // Log issuer.
                        //
                        BIO_write( Out, "  Issuer:\n", 10 );

                        X509IssuerName = X509_get_issuer_name( X509Cert );
                        if (X509IssuerName != NULL)
                        {
                            X509_NAME_print_ex( Out, X509IssuerName, 4, XN_FLAG_MULTILINE );
                        }
                        else
                        {
                            BIO_write( Out, "    Not specified!\n", 19 );
                        }

                        //
                        // Log subject.
                        //
                        BIO_write( Out, "\n  Subject:\n", 12 );

                        X509SubjectName = X509_get_subject_name( X509Cert );
                        if (X509SubjectName != NULL)
                        {
                            X509_NAME_print_ex( Out, X509SubjectName, 4, XN_FLAG_MULTILINE );
                        }
                        else
                        {
                            BIO_write( Out, "    Not specified!\n", 19 );
                        }

                        //
                        // Log serial number.
                        //
                        BIO_write( Out, "\n  Serial Number:\n", 18 );

                        SerialNumber = X509_get0_serialNumber( X509Cert );
                        if (SerialNumber != NULL)
                        {
                            BIO_dump_buffer( Out, SerialNumber->data, SerialNumber->length, 4, TRUE, FALSE );
                        }
                        else
                        {
                            BIO_write( Out, "    Not specified!\n", 19 );
                        }

                        //
                        // Log public key.
                        //
                        BIO_write( Out, "  Public Key:\n", 14 );

                        X509PublicKey = X509_get_X509_PUBKEY( X509Cert );
                        if (X509PublicKey != NULL)
                        {
                            if (X509_PUBKEY_get0_param( NULL, &PublicKeyData, &PublicKeyLength, &X509Algorithm, X509PublicKey ))
                            {
                                X509_ALGOR_get0( &AlgorithmType, NULL, NULL, X509Algorithm );

                                Nid = OBJ_obj2nid( AlgorithmType );
                                if (Nid == NID_rsaEncryption)
                                {
                                    PublicKey = d2i_RSAPublicKey( NULL, &PublicKeyData, PublicKeyLength );
                                    if (PublicKey != NULL)
                                    {
                                        RSA_get0_key( PublicKey, &ModulusNum, &ExponentNum, NULL );

                                        Modulus = BN_to_ASN1_INTEGER( ModulusNum, NULL );
                                        if (Modulus != NULL)
                                        {
                                            BIO_dump_buffer( Out, Modulus->data, Modulus->length, 4, TRUE, FALSE );

                                            ASN1_INTEGER_free( Modulus );
                                        }

                                        RSA_free( PublicKey );
                                    }
                                }
                                else
                                {
                                    BIO_write( Out, "    Not an RSA key!\n", 20 );
                                }
                            }
                            else
                            {
                                BIO_write( Out, "    Invalid parameters!\n", 24 );
                            }
                        }
                        else
                        {
                            BIO_write( Out, "    Not specified!\n", 19 );
                        }

                        BIO_write( Out, "\n\n", 2 );
                    }
                }

                PKCS7_free( PKCS7Object );
            }        
        }
        else
        {
            LogError( "Error occurred getting image certificate data: %d", GetLastError( ) );
        }

        HeapFree( GetProcessHeap( ), 0, Certificate );
    }

#if defined(_DEBUG)
    printf( "press any key to continue..." );
    getchar( );
#endif // _DEBUG

    return 0;
}

