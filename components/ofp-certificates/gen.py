# NOTE:
# Found on gihubt gists, reproduced here as-is
# https://gist.github.com/bloodearnest/9017111a313777b9cce5
# thanks to the author of this !

# NOTE:
# only __main__ function was added to actually generate files

# Copyright 2018 Simon Davy
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# WARNING: the code in the gist generates self-signed certs, for the purposes of testing in development.
# Do not use these certs in production, or You Will Have A Bad Time.
#
# Caveat emptor
#

import os
import argparse
from datetime import datetime, timedelta
import ipaddress

def_infile = 'info.txt'
def_outdir = '.'
def_crt = 'autosign.crt'
def_key = 'autosign.key'


def generate_selfsigned_cert(hostname, ip_addresses=None, key=None):
    """Generates self signed certificate for a hostname, and optional IP addresses."""
    from cryptography import x509
    from cryptography.x509.oid import NameOID
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import rsa

    # Generate our key
    if key is None:
        key = rsa.generate_private_key(
            public_exponent=65537,
            key_size=2048,
            backend=default_backend(),
        )

    name = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, hostname)
    ])

    # best practice seem to be to include the hostname in the SAN, which *SHOULD* mean COMMON_NAME is ignored.
    alt_names = [x509.DNSName(hostname)]

    # allow addressing by IP, for when you don't have real DNS (common in most testing scenarios
    if ip_addresses:
        for addr in ip_addresses:
            # openssl wants DNSnames for ips...
            alt_names.append(x509.DNSName(addr))
            # ... whereas golang's crypto/tls is stricter, and needs IPAddresses
            # note: older versions of cryptography do not understand ip_address objects
            alt_names.append(x509.IPAddress(ipaddress.ip_address(addr)))

    san = x509.SubjectAlternativeName(alt_names)

    # path_len=0 means this cert can only sign itself, not other certs.
    basic_contraints = x509.BasicConstraints(ca=True, path_length=0)
    now = datetime.utcnow()
    cert = (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(1000)
        .not_valid_before(now)
        .not_valid_after(now + timedelta(days=10*365))
        .add_extension(basic_contraints, False)
        .add_extension(san, False)
        .sign(key, hashes.SHA256(), default_backend())
    )
    cert_pem = cert.public_bytes(encoding=serialization.Encoding.PEM)
    key_pem = key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption(),
    )

    return cert_pem, key_pem

def app(infile, outdir):
    hostname = None
    ip_addresses = []

    with open(infile, encoding='utf-8') as f:
        first = True
        for line in f:
            for word in line.split():
                if first:
                    hostname = word
                    first = False
                else:
                    ip_addresses.append(word)

    if not hostname:
        return

    cert_pem, key_pem = generate_selfsigned_cert(hostname, ip_addresses)

    out = os.path.join(outdir, def_crt)
    with open(out, mode='wb') as f:
        f.write(cert_pem)
    out = os.path.join(outdir, def_key)
    with open(out, mode='wb') as f:
        f.write(key_pem)


if __name__ == '__main__':
    try:
        parser = argparse.ArgumentParser(
            description='Generate a self-signed certificate')
        parser.add_argument('--infile', default=def_infile,
                            help='input information file')
        parser.add_argument('--outdir', default=def_outdir,
                            help='where to generate files')
        args = parser.parse_args()
        app(args.infile, args.outdir)
    except Exception as e:
        print(e)
