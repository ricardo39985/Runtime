FROM node:24-bookworm-slim
WORKDIR /app
RUN npm install --global pnpm@10.11.0
COPY package.json pnpm-workspace.yaml pnpm-lock.yaml ./
COPY services/bridge ./services/bridge
RUN pnpm install --prod --frozen-lockfile --filter @runtime/bridge
ENV BIND_ADDRESS=0.0.0.0 PORT=8081
USER node
EXPOSE 8081
CMD ["node","--experimental-strip-types","services/bridge/src/server.ts"]
