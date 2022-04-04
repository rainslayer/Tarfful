/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "microtar.h"

static unsigned round_up(unsigned n, unsigned incr) {
    return n + (incr - n % incr) % incr;
}

static unsigned checksum(const Microtar::raw_header_t *rh) {
    unsigned i;
    unsigned char *p = (unsigned char *) rh;
    unsigned res = 256;
    for (i = 0; i < offsetof(Microtar::raw_header_t, checksum); i++) {
        res += p[i];
    }
    for (i = offsetof(Microtar::raw_header_t, type); i < sizeof(*rh); i++) {
        res += p[i];
    }
    return res;
}


static int tread(Microtar::type *tar, void *data, unsigned size) {
    int err = tar->read(tar, data, size);
    tar->pos += size;
    return err;
}


static int twrite(Microtar::type *tar, const void *data, unsigned size) {
    int err = tar->write(tar, data, size);
    tar->pos += size;
    return err;
}


static int write_null_bytes(Microtar::type *tar, int n) {
    int i, err;
    char nul = '\0';
    for (i = 0; i < n; i++) {
        err = twrite(tar, &nul, 1);
        if (err) {
            return err;
        }
    }
    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


static int raw_to_header(Microtar::header_t *h, const Microtar::raw_header_t *rh) {
    unsigned chksum1, chksum2;

    /* If the checksum starts with a null byte we assume the record is NULL */
    if (*rh->checksum == '\0') {
        return static_cast<int>(Microtar::EStatus::ENULLRECORD);
    }

    /* Build and compare checksum */
    chksum1 = checksum(rh);
    sscanf(rh->checksum, "%o", &chksum2);
    if (chksum1 != chksum2) {
        return static_cast<int>(Microtar::EStatus::EBADCHKSUM);
    }

    /* Load raw header into header */
    sscanf(rh->mode, "%o", &h->mode);
    sscanf(rh->owner, "%o", &h->owner);
    sscanf(rh->size, "%o", &h->size);
    sscanf(rh->mtime, "%o", &h->mtime);
    h->type = rh->type;
    strcpy(h->name, rh->name);
    strcpy(h->linkname, rh->linkname);

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


static int header_to_raw(Microtar::raw_header_t *rh, const Microtar::header_t *h) {
    unsigned chksum;

    /* Load header into raw header */
    memset(rh, 0, sizeof(*rh));
    sprintf(rh->mode, "%o", h->mode);
    sprintf(rh->owner, "%o", h->owner);
    sprintf(rh->size, "%o", h->size);
    sprintf(rh->mtime, "%o", h->mtime);
    rh->type = h->type ? h->type : static_cast<char>(Microtar::EType::TREG);
    strcpy(rh->name, h->name);
    strcpy(rh->linkname, h->linkname);

    /* Calculate and write checksum */
    chksum = checksum(rh);
    sprintf(rh->checksum, "%06o", chksum);
    rh->checksum[7] = ' ';

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


const char *Microtar::Microtar::strerror(int err) {
    switch (err) {
        case static_cast<int>(EStatus::ESUCCESS):
            return "success";
        case static_cast<int>(EStatus::EFAILURE):
            return "failure";
        case static_cast<int>(EStatus::EOPENFAIL):
            return "could not open";
        case static_cast<int>(EStatus::EREADFAIL):
            return "could not read";
        case static_cast<int>(EStatus::EWRITEFAIL):
            return "could not write";
        case static_cast<int>(EStatus::ESEEKFAIL):
            return "could not seek";
        case static_cast<int>(EStatus::EBADCHKSUM):
            return "bad checksum";
        case static_cast<int>(EStatus::ENULLRECORD):
            return "null record";
        case static_cast<int>(EStatus::ENOTFOUND):
            return "file not found";
    }
    return "unknown error";
}


static int file_write(Microtar::type *tar, const void *data, unsigned size) {
    unsigned res = fwrite(data, 1, size, tar->stream);
    return (res == size) ? static_cast<int>(Microtar::EStatus::ESUCCESS)
                         : static_cast<int>(Microtar::EStatus::EWRITEFAIL);
}

static int file_read(Microtar::type *tar, void *data, unsigned size) {
    unsigned res = fread(data, 1, size, tar->stream);
    return (res == size) ? static_cast<int>(Microtar::EStatus::ESUCCESS)
                         : static_cast<int>(Microtar::EStatus::EREADFAIL);
}

static int file_seek(Microtar::type *tar, unsigned offset) {
    int res = fseek(tar->stream, offset, SEEK_SET);
    return (res == 0) ? static_cast<int>(Microtar::EStatus::ESUCCESS)
                      : static_cast<int>(Microtar::EStatus::ESEEKFAIL);
}

static int file_close(Microtar::type *tar) {
    fclose(tar->stream);
    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


int Microtar::Microtar::open(type *tar, const char *filename, const char *mode) {
    int err;
    header_t h;

    /* Init tar struct and functions */
    memset(tar, 0, sizeof(*tar));
    tar->write = file_write;
    tar->read = file_read;
    tar->seek = file_seek;
    tar->close = file_close;

    /* Assure mode is always binary */
    if (strchr(mode, 'r')) mode = "rb";
    if (strchr(mode, 'w')) mode = "wb";
    if (strchr(mode, 'a')) mode = "ab";
    /* Open file */
    tar->stream = fopen(filename, mode);
    if (!tar->stream) {
        return static_cast<int>(EStatus::EOPENFAIL);
    }
    /* Read first header to check it is valid if mode is `r` */
    if (*mode == 'r') {
        err = Microtar::read_header(tar, &h);
        if (err != static_cast<int>(EStatus::ESUCCESS)) {
            Microtar::close(tar);
            return err;
        }
    }

    /* Return ok */
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::close(type *tar) {
    return tar->close(tar);
}


int Microtar::Microtar::seek(type *tar, unsigned pos) {
    int err = tar->seek(tar, pos);
    tar->pos = pos;
    return err;
}


int Microtar::Microtar::rewind(type *tar) {
    tar->remaining_data = 0;
    tar->last_header = 0;
    return Microtar::Microtar::seek(tar, 0);
}


int Microtar::Microtar::next(type *tar) {
    int err, n;
    header_t h;
    /* Load header */
    err = Microtar::Microtar::read_header(tar, &h);
    if (err) {
        return err;
    }
    /* Seek to next record */
    n = round_up(h.size, 512) + sizeof(raw_header_t);
    return Microtar::Microtar::seek(tar, tar->pos + n);
}


int Microtar::Microtar::find(type *tar, const char *name, header_t *h) {
    int err;
    header_t header;
    /* Start at beginning */
    err = Microtar::Microtar::rewind(tar);
    if (err) {
        return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while ((err = Microtar::Microtar::read_header(tar, &header)) == static_cast<int>(EStatus::ESUCCESS)) {
        if (!strcmp(header.name, name)) {
            if (h) {
                *h = header;
            }
            return static_cast<int>(EStatus::ESUCCESS);
        }
        Microtar::Microtar::next(tar);
    }
    /* Return error */
    if (err == static_cast<int>(EStatus::ENULLRECORD)) {
        err = static_cast<int>(EStatus::ENOTFOUND);
    }
    return err;
}


int Microtar::Microtar::read_header(type *tar, header_t *h) {
    int err;
    raw_header_t rh;
    /* Save header position */
    tar->last_header = tar->pos;
    /* Read raw header */
    err = tread(tar, &rh, sizeof(rh));
    if (err) {
        return err;
    }
    /* Seek back to start of header */
    err = Microtar::Microtar::seek(tar, tar->last_header);
    if (err) {
        return err;
    }
    /* Load raw header into header struct and return */
    return raw_to_header(h, &rh);
}


int Microtar::Microtar::read_data(type *tar, void *ptr, unsigned size) {
    int err;
    /* If we have no remaining data then this is the first read, we get the size,
     * set the remaining data and seek to the beginning of the data */
    if (tar->remaining_data == 0) {
        header_t h;
        /* Read header */
        err = Microtar::Microtar::read_header(tar, &h);
        if (err) {
            return err;
        }
        /* Seek past header and init remaining data */
        err = Microtar::Microtar::seek(tar, tar->pos + sizeof(raw_header_t));
        if (err) {
            return err;
        }
        tar->remaining_data = h.size;
    }
    /* Read data */
    err = tread(tar, ptr, size);
    if (err) {
        return err;
    }
    tar->remaining_data -= size;
    /* If there is no remaining data we've finished reading and seek back to the
     * header */
    if (tar->remaining_data == 0) {
        return Microtar::Microtar::seek(tar, tar->last_header);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::write_header(type *tar, const header_t *h) {
    raw_header_t rh;
    /* Build raw header and write */
    header_to_raw(&rh, h);
    tar->remaining_data = h->size;
    return twrite(tar, &rh, sizeof(rh));
}


int Microtar::Microtar::write_file_header(type *tar, const char *name, unsigned size) {
    header_t h;
    /* Build header */
    memset(&h, 0, sizeof(h));
    strcpy(h.name, name);
    h.size = size;
    h.type = static_cast<char>(EType::TREG);
    h.mode = 0664;
    /* Write header */
    return Microtar::Microtar::write_header(tar, &h);
}


int Microtar::Microtar::write_dir_header(type *tar, const char *name) {
    header_t h;
    /* Build header */
    memset(&h, 0, sizeof(h));
    strcpy(h.name, name);
    h.type = static_cast<char>(EType::TDIR);
    h.mode = 0775;
    /* Write header */
    return Microtar::Microtar::write_header(tar, &h);
}


int Microtar::Microtar::write_data(type *tar, const void *data, unsigned size) {
    int err;
    /* Write data */
    err = twrite(tar, data, size);
    if (err) {
        return err;
    }
    tar->remaining_data -= size;
    /* Write padding if we've written all the data for this file */
    if (tar->remaining_data == 0) {
        return write_null_bytes(tar, round_up(tar->pos, 512) - tar->pos);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::finalize(type *tar) {
    /* Write two NULL records */
    return write_null_bytes(tar, sizeof(raw_header_t) * 2);
}
