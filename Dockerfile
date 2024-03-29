FROM rust:bullseye

RUN apt update
RUN apt install -y sudo curl wget
RUN /bin/bash -c "$(curl -sL https://git.io/vokNn)"
RUN apt-fast install -y clang cmake git
ADD . /synthizer
WORKDIR /synthizer
ENTRYPOINT ./ci/build_linux.sh
