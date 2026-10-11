# Ubuntu 服务器部署

本目录提供 Ubuntu 24.04 的 systemd 和 Nginx 配置。C++ 服务监听 `127.0.0.1:18080`，Nginx 通过 HTTPS 提供页面和 API；SQLite 数据库独立保存在 `/var/lib/school-personnel/school.db`。域名模板使用 `swustmarket.xyz`，迁移服务器时需修改域名和证书路径。

2026 年 10 月 11 日首次部署到 [https://swustmarket.xyz](https://swustmarket.xyz)，公网验证完成。后端、Nginx 和 `certbot.timer` 均已启用开机启动；证书模拟续期通过，续期后的 Nginx reload 钩子也已运行验证。首次部署的空数据库备份位于 `/var/backups/school-personnel/school-20261011-initial.db`。

同日已更新为教师、行政人员、学生、普通工作者四类人员版本，当前发布目录是 `/opt/school-personnel/releases/20261011-four-types`。Windows 和 Ubuntu 的 16 组接口测试及控制台测试均通过。更新前后数据库备份分别为 `/var/backups/school-personnel/school-20261011-before-four-types.db` 和 `/var/backups/school-personnel/school-20261011-after-four-types.db`；原有人员记录经逐字段比对保留，公网新增类别的验证记录已删除。网站账号和密码保持原配置。

## 目录与服务

| 路径 | 用途 |
| --- | --- |
| `/opt/school-personnel/releases/<版本>/` | 每次发布的源码、前端和 Linux 编译产物 |
| `/opt/school-personnel/current` | 当前版本的符号链接 |
| `/var/lib/school-personnel/school.db` | 正式数据库及同目录 WAL 文件 |
| `/etc/school-personnel/environment` | 后端环境配置，root 所有、权限 `0600` |
| `/etc/systemd/system/school-personnel.service` | 后端开机启动与故障重启 |
| `/etc/nginx/sites-available/school-personnel.conf` | 网站反向代理配置 |
| `/var/www/school-personnel-acme` | HTTPS 证书验证目录 |
| `/etc/letsencrypt/live/swustmarket.xyz/` | 证书和私钥，仅保存在服务器 |

实际密码、私钥和数据库不提交到 Git。网站登录使用独立账号密码；不要将 SSH 密码作为网站密码。

## 首次部署步骤

以下操作由管理员在 Ubuntu 上执行。先检查现有网站和服务，避免覆盖其他项目；宝塔自带的 Nginx 与系统 Nginx 应选择一个运行。本配置针对 `/usr/sbin/nginx` 和 `/etc/nginx`。

1. 安装 `g++`、`gcc`、`cmake`、`make`、`nginx`、`certbot`、`python3`。项目已自带 SQLite、JSON 和 HTTP 库源码。
2. 将对应 Git 版本的源码放入新的发布目录，执行：

   ```bash
   cmake -S /opt/school-personnel/releases/<版本> \
     -B /opt/school-personnel/releases/<版本>/build-cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build /opt/school-personnel/releases/<版本>/build-cmake -j1
   cd /opt/school-personnel/releases/<版本>
   python3 tests/test_api.py --server build-cmake/server
   python3 tests/test_console.py --console build-cmake/school_system
   ```

3. 创建无登录权限的系统用户 `school-personnel`，将 `/var/lib/school-personnel` 的所有权交给该用户。发布目录保持 root 所有，只允许服务读取和执行。
4. 将 `current` 链接指向已测试的新版本。根据 `environment.example` 创建服务器环境配置，填写独立随机密码并设为 `0600`；不要覆盖已有环境配置或数据库。
5. 安装 `school-personnel.service` 到 `/etc/systemd/system/`，执行：

   ```bash
   systemctl daemon-reload
   systemctl enable --now school-personnel
   curl --fail http://127.0.0.1:18080/api/health
   ```

6. 放行阿里云安全组的 TCP `80` 和 `443`，确认域名 A 记录指向服务器。后端端口 `18080` 只监听本机，无需公网放行。
7. 先安装 `nginx.http.conf` 为网站配置，建立对应 `sites-enabled` 链接，执行 `nginx -t` 后 reload。初始配置只开放证书验证和部署状态，尚不传输网站登录凭据。
8. 获取证书：

   ```bash
   certbot certonly --webroot -w /var/www/school-personnel-acme \
     -d swustmarket.xyz --cert-name swustmarket.xyz --agree-tos \
     --register-unsafely-without-email --non-interactive
   ```

   此方式未登记联系邮箱，证书到期提醒依赖服务器运维；也可使用自己的邮箱替代 `--register-unsafely-without-email`。

9. 用 `nginx.https.conf` 替换初始配置，运行 `nginx -t` 再 reload。将 `renew-nginx.sh` 安装到 `/etc/letsencrypt/renewal-hooks/deploy/school-personnel-nginx.sh`，权限 `0755`，启用 `certbot.timer`。通过 `certbot renew --dry-run` 验证续期。
10. 验证 HTTP 跳转 HTTPS、未登录返回 `401`、登录后页面和增删改查正常，以及后端重启后记录仍存在。

## 日常运维

```bash
systemctl status school-personnel nginx
journalctl -u school-personnel -n 50 --no-pager
nginx -t
systemctl restart school-personnel
systemctl list-timers certbot.timer
```

修改网站登录密码时，使用 `sudoedit /etc/school-personnel/environment` 编辑 `SCHOOL_ADMIN_PASSWORD`，保持文件 `0600`，然后重启 `school-personnel`。浏览器可能缓存 Basic 登录信息，修改后可使用隐私窗口重新登录。

数据备份使用 SQLite 备份接口，可在后端运行时执行：

```bash
umask 077
python3 /opt/school-personnel/current/tools/backup_database.py \
  --source /var/lib/school-personnel/school.db \
  --output /var/backups/school-personnel/school-$(date +%Y%m%d-%H%M%S).db
```

不要仅复制正在运行的 `.db` 文件，也不要删除 `.db-wal`。更新代码时先备份数据库，将新源码放入新的发布目录，编译并通过测试后更新 `current`，再重启服务。数据库和环境配置无需随发布复制。

学生与普通工作者版本会自动扩展现有 `people` 表，保留原来的教师、行政人员记录。更新前需备份正式数据库。旧版程序无法识别 `Student` / `Worker` 记录；添加这两类记录后，不能仅切换回旧程序。若确需回退旧版，需停机恢复更新前的完整 SQLite 备份，并明确这会恢复到备份时的数据状态；应先额外备份当前数据库。
