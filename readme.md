cat > README.md <<'EOF'
# RemoteOps - IE3090

## Student
IT24101220

## Project
RemoteOps: A Remote System Monitoring and Management Tool over TCP/IP.

## Programs
- Agent: `agent_220`
- Controller: `controller_220`

## Network Configuration
- TCP Port: `9410`
- Session ID: `0221`
- Authentication Token: `OPS-1220`

## Supported Commands
- `AUTH <token>`
- `SYSINFO`
- `LISTPROC`
- `EXEC DATE`
- `EXEC UPTIME`
- `EXEC DISKFREE`
- `EXEC HOSTNAME`
- `EXEC WHOAMI`
- `PUT <filename> <size>`
- `GET <filename>`
- `MONITOR START <udp_port>`
- `MONITOR STOP`
- `QUIT`

## Build

```bash
make -f Makefile_220
