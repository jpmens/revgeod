FROM debian:bullseye-slim

WORKDIR /usr/src/app

RUN apt-get update \
     && apt-get install -y --no-install-recommends build-essential jq liblmdb-dev lmdb-utils curl libcurl4-openssl-dev ca-certificates openssl libmicrohttpd12 libmicrohttpd-dev \
     && mkdir -p data/geocache

COPY . .

RUN cp config.mk.example config.mk \
    && make install

EXPOSE 8865

CMD ["/usr/local/sbin/revgeod"]