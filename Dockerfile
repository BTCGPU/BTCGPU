# To build :
# $ docker build -t btcgpu:latest .
# To run :
# docker run -v /home/user/btcgold:/root/.bitcoingold -d btcgpu:latest 

FROM debian:sid as builder

RUN apt-get update && apt-get install -y gnupg && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D46F45428842CE5E
RUN echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin-ubuntu-bitcoin-xenial.list

RUN apt-get update && apt-get install -y git build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils

#RUN apt-get install -y libboost-all-dev libboost-mpi-python-dev 
RUN apt-get install -y libdb4.8-dev libdb4.8++-dev
RUN apt-get install -y libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
RUN apt-get install -y libzmq3-dev

RUN git clone https://github.com/BTCGPU/BTCGPU
RUN cd BTCGPU && ./autogen.sh && ./configure --disable-shared && make

WORKDIR "/BTCGPU/src/"
RUN ls -al
RUN ldd ./bitcoind
RUN ldd ./bitcoin-cli
RUN ldd ./bitcoin-tx

FROM debian:sid

RUN apt-get update && apt-get install -y gnupg && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D46F45428842CE5E
RUN echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin-ubuntu-bitcoin-xenial.list
RUN apt-get update && apt-get install -y libdb4.8++ libssl1.0 libzmq5 libevent-pthreads-2.1-6 libevent-2.1-6 libboost-chrono1.62.0 libboost-system1.62.0 libboost-filesystem1.62.0 libboost-thread1.62.0 libboost-program-options1.62.0

COPY --from=builder /BTCGPU/src/bitcoind /usr/bin/bitcoingd
COPY --from=builder /BTCGPU/src/bitcoin-cli /usr/bin/bitcoing-cli
COPY --from=builder /BTCGPU/src/bitcoin-tx /usr/bin/bitcoing-tx

CMD ["bitcoind"]
