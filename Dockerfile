FROM espressif/idf:v5.1.2

SHELL ["/bin/bash", "-lc"]

ENV I4A_REPO_ROOT=/workspace
ENV I4A_APP_DIR=/workspace/app

WORKDIR /workspace/app

COPY docker/i4a-container.sh /usr/local/bin/i4a-container

RUN chmod +x /usr/local/bin/i4a-container \
    && mkdir -p /workspace

ENTRYPOINT ["/usr/local/bin/i4a-container"]
CMD ["bash"]
