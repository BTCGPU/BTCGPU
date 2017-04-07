FROM debian:jessie
MAINTAINER slush@satoshilabs.com

ENV DEBIAN_FRONTEND noninteractive
ENV HOME /root
ENV TERM linux

RUN apt-get update && \
    apt-get upgrade -qy && \
    apt-get install -qy apt-transport-https curl git && \
    echo 'deb https://deb.nodesource.com/node_4.x jessie main' | tee /etc/apt/sources.list.d/nodesource.list && \
    curl -s https://deb.nodesource.com/gpgkey/nodesource.gpg.key | apt-key add -
RUN apt-get update && \
    apt-get -qy install nodejs python make build-essential libzmq3-dev devscripts dh-systemd
RUN apt-get -qy install libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
RUN apt-get -qy install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
RUN git clone 'https://github.com/satoshilabs/bitcoin.git' && cd bitcoin
RUN cd bitcoin && ./autogen.sh
RUN cd bitcoin && ./configure --disable-wallet
RUN cd bitcoin && make
ADD dist.sh /dist.sh
RUN ./dist.sh
