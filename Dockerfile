FROM ubuntu:18.04
MAINTAINER Max Conway

RUN apt-get update && \
    apt-get install -y make build-essential python3 software-properties-common git python3-venv python3-dev libi2c-dev

COPY .git /data/.git
RUN git -C /data/ reset --hard

WORKDIR /data/
RUN ./build.sh 