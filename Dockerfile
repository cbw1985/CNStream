#/*************************************************************************
# * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
# *
# *  Licensed under the Apache License, Version 2.0 (the "License");
# *  you may not use this file except in compliance with the License.
# *  You may obtain a copy of the License at
# *
# *     http://www.apache.org/licenses/LICENSE-2.0
# *
# * The above copyright notice and this permission notice shall be included in
# * all copies or substantial portions of the Software.
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# * THE SOFTWARE.
# *************************************************************************/

# 1. put your neuware package under the directory of CNStream
# 2. docker build -f Dockerfile --build-arg mlu_platform=${board_series} --build-arg neuware_package=${neuware_package_name} -t ubuntu_cnstream:v1 .
# 3. docker run -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --privileged -v /dev:/dev --net=host --pid=host -v $HOME/.Xauthority -it --name container_name  -v $PWD:/workspace ubuntu_cnstream:v1

FROM ubuntu:16.04

MAINTAINER <Cambricon, Inc.>

WORKDIR /root/CNStream/

ARG neuware_package=neuware-standard-8.1.1.deb
ARG mlu_platform=MLU100

RUN echo -e 'nameserver 114.114.114.114' > /etc/resolv.conf

RUN echo deb http://mirrors.aliyun.com/ubuntu/ xenial main restricted  > /etc/apt/sources.list && \
    echo deb-src http://mirrors.aliyun.com/ubuntu/ xenial main restricted multiverse universe >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-updates main restricted >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial universe >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-updates universe >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial multiverse >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-updates multiverse >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-backports main restricted universe multiverse >> /etc/apt/sources.list && \
    echo deb-src http://mirrors.aliyun.com/ubuntu/ xenial-backports main restricted universe multiverse >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-security main restricted >> /etc/apt/sources.list && \
    echo deb-src http://mirrors.aliyun.com/ubuntu/ xenial-security main restricted multiverse universe >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-security universe  >> /etc/apt/sources.list && \
    echo deb http://mirrors.aliyun.com/ubuntu/ xenial-security multiverse >> /etc/apt/sources.list && \
    apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends \
            curl git wget vim build-essential cmake make \
            libopencv-dev \
            libgoogle-glog-dev \
            openssh-server \
            net-tools && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY . /root/CNStream/

RUN dpkg -i /root/CNStream/$neuware_package && \
    mkdir build && cd build && cmake .. -DMLU=$mlu_platform && make -j8 && \
    rm -rf /root/CNStream/$neuware_package

WORKDIR /root/CNStream/samples/demo

