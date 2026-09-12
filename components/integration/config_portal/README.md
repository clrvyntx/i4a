# Portal cautivo de configuración

En modo configuración, conectarse a `ComNetAR_Config`. DHCP anuncia
`192.168.4.1` como DNS. El servidor DNS local responde consultas A con esa IP;
las consultas de otros tipos (incluido AAAA) reciben una respuesta sin registros.
Las consultas malformadas o con más de una pregunta se descartan.

Las rutas HTTP GET desconocidas, incluidas las comprobaciones de portal cautivo,
redirigen a `http://192.168.4.1/`. La página principal permite cambiar la contraseña
Wi-Fi y ofrece «Acceder a configuración», que abre `/admin` y conserva el login
administrativo. HTTPS no se intercepta. La apertura automática depende del cliente;
el acceso manual por IP sigue disponible.

DNS arranca únicamente en modo configuración y se detiene al cerrar el portal.
No se modifican el hook de enrutamiento ni los filtros de entrada.

## Verificación en equipo

1. Entrar al modo configuración y conectar un celular al AP. Comprobar la apertura
   del portal o abrir `http://192.168.4.1/` manualmente.
2. Consultar `nslookup example.com 192.168.4.1`: debe responder `192.168.4.1`.
   Una consulta AAAA debe finalizar sin direcciones IPv6.
3. Abrir `/generate_204`, `/hotspot-detect.html` y `/connecttest.txt`: deben
   redirigir a la página principal.
4. Pulsar «Acceder a configuración»: debe pedir la contraseña administrativa.
   Verificar inicio y cierre de sesión, y el formulario de contraseña Wi-Fi.
5. Dejar vencer los cinco minutos: comprobar el cierre del portal y la salida
   del modo configuración según el comportamiento existente.
6. Volver al modo normal y comprobar conectividad habitual. Repetir la entrada
   a configuración para verificar que DNS y HTTP vuelven a arrancar.
