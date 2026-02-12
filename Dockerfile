FROM alpine:latest AS builder
ARG BUILD_MODE
ARG STEAM_APP_ID
ARG UNET_MODULE_STEAM

RUN echo "Building oservice please wait..."
RUN echo "------------------------------------"
RUN echo "Build Mode: ${BUILD_MODE}"
RUN echo "Steam App Id: ${STEAM_APP_ID}"
RUN echo "Unet Module Steam: ${UNET_MODULE_STEAM}"
RUN echo "------------------------------------"

RUN apk add --no-cache \
	    build-base \
	    git \
	    cmake \
	    patchelf \
	    ca-certificates \
	    bash \
	    binutils \
        util-linux-dev

WORKDIR /build

# Copy source code
COPY . .

#This is needed because of patchelf and alpine
ENV CFLAGS="-DHAS_SOCKLEN_T=1 -D_GNU_SOURCE"
ENV CXXFLAGS="-DHAS_SOCKLEN_T=1 -D_GNU_SOURCE"
RUN chmod +x build-unix && ./build-unix --${BUILD_MODE}

RUN TARGET_SO=$(find . -name "oservice.so" | head -n 1) && \
    patchelf --set-rpath '$ORIGIN' "$TARGET_SO" && \
    mkdir -p /build/dist && \
    mv "$TARGET_SO" /build/dist/oservice.so

CMD ["cp", "/build/dist/oservice.so", "/build/out/"]
