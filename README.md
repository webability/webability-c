# webability-c

Cliente oficial en C para conectarse a los servicios de [WebAbility](https://www.webability.info) — la plataforma que ofrece DNS gestionado, procesamiento de imágenes/CDN, envío de correo transaccional y (próximamente) transcodificación de video y email marketing masivo, todos expuestos a través de una única API HTTP en `https://api.webability.info`.

🚧 **En construcción.** Es el equivalente en C de [webability-go](https://github.com/webability/webability-go) (implementación de referencia) — mismo esquema de autenticación (ClientID + Token, firma HMAC-SHA256, headers `X-WA-Client`/`X-WA-Timestamp`/`X-WA-Digest`) y mismos endpoints (`dns`, `image`, `mail`, y próximamente `video`/`marketing`).

C no tiene un gestor de paquetes universal, así que esta librería se distribuye como fuente (y opcionalmente vía vcpkg/Conan más adelante).

## Build

```bash
make
```

## Servicios disponibles

| Servicio    | Estado                                                        |
|-------------|----------------------------------------------------------------|
| DNS         | 🚧 Pendiente de portar desde webability-go                     |
| Imágenes    | 🚧 Pendiente de portar desde webability-go                     |
| Mail        | 🚧 Pendiente de portar desde webability-go                     |
| Video       | 🚧 Borrador solamente en webability-go, aún sin servidor real  |
| Marketing   | 🚧 Borrador solamente en webability-go, aún sin servidor real  |

## Documentación de la API

- https://www.webability.info/documentacion/dns
- https://www.webability.info/documentacion/imagenes
- https://www.webability.info/documentacion/mailing
- https://www.webability.info/documentacion/video

## Estado

Repositorio reservado — implementación en progreso. Ver [webability-go](https://github.com/webability/webability-go) para el contrato completo de la API mientras tanto.

## Licencia

MIT — ver [LICENSE](LICENSE).
