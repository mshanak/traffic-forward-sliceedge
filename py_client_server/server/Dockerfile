FROM python:3.12-alpine
WORKDIR /app
RUN apk add --no-cache netcat-openbsd
COPY server.py .
CMD ["python", "server.py"]
