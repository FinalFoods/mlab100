# mlab

This component is a holding place for application agnostic Microbiota
Labs code which may need to be shared between different applications
or target platform images.

## HTTPS daemon certificates

The certs files were created using:

```
openssl req -newkey rsa:2048 -nodes -keyout prvtkey.pem -x509 -days 3650 -out cacert.pem -subj "/CN=Microbiota Labs HTTPS server"
```

**NOTE**: Ideally we would NOT be storing the private keys in the
source repository, but would acquire the file via a secure mechanism
during the build process. The security for the device still needs to
be fully addressed (i.e. using secure-boot, encrypted flash and giving
each device a factory-set unique identity (and more than likely a
unique cert/key pair per-device).
