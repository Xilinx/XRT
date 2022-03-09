ARG BASE_IMAGE=ubuntu:18.04
FROM $BASE_IMAGE

ENV LANG C.UTF-8
ENV LC_ALL=C
ENV DEBIAN_FRONTEND="noninteractive"
COPY xrtdeps.sh .

RUN apt-get update \
    && ./xrtdeps.sh -docker \
    && rm -rf /var/lib/apt/lists/* xrtdeps.sh;
