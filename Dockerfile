FROM ubuntu@sha256:c95a8e48bf88e9849f3e0f723d9f49fa12c5a00cfc6e60d2bc99d87555295e4c AS builder

WORKDIR /build

RUN apt update
RUN apt install -y --no-install-recommends make pkg-config python3 python3-pip
RUN pip3 install imageio pywavefront

RUN apt install -y --no-install-recommends wget gnupg2
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN printf "\ndeb http://apt.llvm.org/focal/ llvm-toolchain-focal main\n" >> /etc/apt/sources.list
RUN apt update
RUN apt install -y --no-install-recommends clang libc++-12-dev libc++abi-12-dev
RUN apt install -y --no-install-recommends libglm-dev libsfml-dev
COPY . /build/QuakeOats
RUN make -C QuakeOats all

FROM ubuntu@sha256:c95a8e48bf88e9849f3e0f723d9f49fa12c5a00cfc6e60d2bc99d87555295e4c

WORKDIR /QuakeOats

RUN apt update
RUN apt install -y --no-install-recommends libsfml-dev wget gnupg2 ca-certificates
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN printf "\ndeb http://apt.llvm.org/focal/ llvm-toolchain-focal main\n" >> /etc/apt/sources.list
RUN apt update && apt install -y --no-install-recommends libc++-12-dev libc++abi-12-dev
COPY --from=builder /build/QuakeOats/QuakeOats /QuakeOats/
COPY --from=builder /build/QuakeOats/assets /QuakeOats/

CMD ["./QuakeOats"]
