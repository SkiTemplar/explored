# Tools/Nube — lanzador de sesiones en la nube

El crédito de «sesiones en la nube» de Claude Code (250 US$ en Max, caduca el 5 de noviembre de 2026) **solo** lo consumen las sesiones en la nube que se inician a mano. Hay tres formas de iniciarlas:

- desde claude.ai/code;
- desde la app de escritorio, en modo Cloud;
- con `claude --cloud`.

Las **rutinas programadas NO consumen el crédito**: se cobran del plan. Por eso existe este lanzador.

## Uso

1. Escribe cada encargo como un `.md` en `cola/`. `comun.md` se antepone a todos.
2. Ejecuta `powershell -File Tools/Nube/lanzar.ps1 [-Max 3] [-DryRun]` **a mano, en una terminal propia** (Windows Terminal o PowerShell). `claude --cloud` exige una terminal interactiva: no funciona desde tareas programadas, desde tuberías ni desde otra sesión de Claude.

El script lanza las N primeras tareas de la cola, cada una en su propia sesión en la nube, y las mueve a `lanzadas/`. Cada sesión tiene 4 vCPU y 16 GB, y no tiene Unreal.

Cada sesión abre su PR. El integrador en la nube, una rutina que corre cada 2 h, fusiona las que puede verificar. A las demás les pone la etiqueta `necesita-unreal`, y esas se integran en una sesión local.

## Seguimiento

- Las sesiones se siguen en claude.ai/code.
- El saldo del crédito aparece en los ajustes de uso de la cuenta.
- Para gastar el crédito antes de que caduque, hay que ir a unos 45 US$ por semana.
