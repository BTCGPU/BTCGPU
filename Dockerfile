# To build :
# $ docker build -t btcgpu:latest .
# To run :
# docker run -v /home/user/btcgold:/home/bitcoingold/.bitcoingold -d btcgpu:latest 

FROM debian:sid as builder

RUN apt-get update && apt-get install -y gnupg && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D46F45428842CE5E
RUN echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin-ubuntu-bitcoin-xenial.list

RUN apt-get update && apt-get install -y git build-essential libtool autotools-dev automake pkg-config \
    libssl-dev libevent-dev bsdmainutils libdb4.8-dev libdb4.8++-dev \
    libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev \
    libboost-test-dev libboost-thread-dev libzmq3-dev libsodium-dev

RUN git clone https://github.com/BTCGPU/BTCGPU
RUN cd BTCGPU && ./autogen.sh && ./configure --disable-shared && make

WORKDIR "/BTCGPU/src/"
RUN ls -al
RUN ldd ./bgoldd
RUN ldd ./bgold-cli
RUN ldd ./bitcoin-tx

FROM debian:sid

ENV USER_UID ${USER_UID:-8000}
ENV GROUP_UID ${GROUP_UID:-8000}

RUN apt-get update && apt-get install -y gnupg && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D46F45428842CE5E
RUN echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin-ubuntu-bitcoin-xenial.list
RUN apt-get update && apt-get install -y libdb4.8++ libssl1.0 libzmq5 libevent-pthreads-2.1-6 libevent-2.1-6 libboost-chrono1.62.0 libboost-system1.62.0 libboost-filesystem1.62.0 libboost-thread1.62.0 libboost-program-options1.62.0 libsodium18

RUN groupadd -r -g ${GROUP_UID} bitcoingold && useradd -u ${USER_UID} -m -g bitcoingold bitcoingold

COPY --from=builder /BTCGPU/src/bgoldd /usr/bin/bgoldd
COPY --from=builder /BTCGPU/src/bgold-cli /usr/bin/bgold-cli
COPY --from=builder /BTCGPU/src/bitcoin-tx /usr/bin/bitcoing-tx

USER bitcoingold

ADD run.sh /run.sh

CMD ["bash", "run.sh"]
