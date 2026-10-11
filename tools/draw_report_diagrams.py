"""Generate the assignment's class and algorithm diagrams using Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "report-assets"
FONT = Path("C:/Windows/Fonts/msyh.ttc")


def font(size):
    return ImageFont.truetype(str(FONT), size)


def centered(draw, y, text, size=32, width=1640):
    draw.text((width / 2, y), text, fill="#182330", font=font(size), anchor="mt")


def box(draw, rect, title, lines, size=27):
    x1, y1, x2, y2 = rect
    draw.rectangle(rect, outline="#263644", width=3, fill="white")
    draw.text(((x1+x2)/2, y1+15), title, font=font(31), fill="#182330", anchor="mt")
    draw.line((x1, y1+66, x2, y1+66), fill="#263644", width=2)
    for i, line in enumerate(lines):
        draw.text((x1+16, y1+83+i*37), line, font=font(size), fill="#182330")


def inheritance(draw, points, label=None):
    tip = points[-1]
    draw.line(points[:-1] + [(tip[0], tip[1]+30)], fill="#263644", width=3)
    draw.polygon([tip, (tip[0]-16, tip[1]+30), (tip[0]+16, tip[1]+30)],
                 outline="#263644", fill="white", width=3)
    if label:
        draw.text((points[0][0]+10, points[0][1]-70), label, font=font(25), fill="#182330")


def class_diagram():
    im = Image.new("RGB", (1640, 1570), "white")
    d = ImageDraw.Draw(im)
    centered(d, 18, "学校人员类结构图", 40)
    box(d, (540, 105, 1160, 395), "Person  «abstract»", [
        "# id / name / gender / age / phone", "+ virtual getType / display / save",
        "+ virtual ~Person()", "- static atomic<int> liveCount", "+ friend operator<<"], 26)
    box(d, (35, 605, 395, 875), "AdminStaff", ["- department", "- position / salary", "+ override 虚函数"])
    box(d, (440, 605, 800, 875), "Teacher", ["- department", "- title / salary", "+ override 虚函数"])
    box(d, (840, 605, 1200, 875), "Student", ["- department / major", "- className / year", "+ override 虚函数"])
    box(d, (1240, 605, 1600, 875), "Worker", ["- department", "- job / salary", "+ override 虚函数"])
    inheritance(d, [(215,605),(215,460),(610,460),(610,395)])
    inheritance(d, [(620,605),(620,510),(770,510),(770,395)], "virtual public")
    inheritance(d, [(1020,605),(1020,510),(940,510),(940,395)], "virtual public")
    inheritance(d, [(1420,605),(1420,460),(1090,460),(1090,395)])
    box(d, (440, 1190, 880, 1465), "GraduateStudent", ["- researchDirection", "- supervisor", "+ override 冲突的虚函数", "+ salary = 0"])
    box(d, (960, 1190, 1320, 1390), "Undergraduate", ["+ getType() = 本科生", "+ 默认虚析构"])
    inheritance(d, [(555,1190),(555,1035),(620,1035),(620,875)])
    inheritance(d, [(735,1190),(735,985),(1010,985),(1010,875)])
    inheritance(d, [(1140,1190),(1140,875)])
    centered(d, 1500, "空心三角指向父类；研究生的两个父类共享唯一 Person 子对象", 29)
    im.save(OUT / "uml.png")


def arrow(d, x, y1, y2):
    d.line((x,y1,x,y2-15), fill="#263644", width=3)
    d.polygon([(x,y2),(x-10,y2-18),(x+10,y2-18)], fill="#263644")


def flow(filename, title, steps, note):
    width, height = 1400, 250 + len(steps)*155
    im=Image.new("RGB", (width,height), "white")
    d=ImageDraw.Draw(im)
    centered(d, 22, title, 42, width)
    for i,(heading,detail) in enumerate(steps):
        y=105+i*155
        d.rounded_rectangle((150,y,1250,y+110), radius=12, outline="#263644", width=3)
        centered(d,y+12,heading,33,width)
        centered(d,y+61,detail,27,width)
        if i+1<len(steps):arrow(d,700,y+110,y+155)
    centered(d,height-80,note,27,width)
    im.save(OUT / filename)


def main():
    if not FONT.is_file():
        raise SystemExit("Requires the Windows Microsoft YaHei font for Chinese diagrams")
    OUT.mkdir(parents=True,exist_ok=True)
    class_diagram()
    flow("ranking-flow.png", "单门课程成绩排名流程", [
        ("读取本课程已录入的学生成绩", "只包含本科生和研究生；未录入者不参与"),
        ("以百分之一分的整数表示成绩", "例如 95.50 分表示为 9550，避免浮点比较误差"),
        ("std::sort 调用 Grade::operator<", "按成绩降序，同分按学号升序稳定展示"),
        ("遍历排序结果，下标 i 从 0 开始", "初始 rank = 0，previous = -1"),
        ("若当前分数不同于 previous，则 rank = i + 1", "相同分数沿用 rank；更新 previous"),
        ("输出学生编号、姓名、成绩和 rank", "例如 95、95、88 对应名次 1、1、3"),
        ("个人接口只保留本人成绩与名次", "显示本课已录入人数；无成绩时返回空值")
    ], "排序 O(n log n)，名次扫描 O(n)，空间 O(n)")
    flow("login-flow.png", "个人账号登录与资料查询流程", [
        ("接收账号、密码及请求来源", "JSON 格式和来源检查；登录尝试次数限制"),
        ("区分管理员配置与个人账号", "个人密码通过随机盐和 PBKDF2 散列验证"),
        ("密码错误返回 401，成功继续", "不向客户端说明账号是否存在"),
        ("生成随机会话令牌", "数据库保存 SHA256 散列和到期时间"),
        ("返回 HttpOnly 会话 Cookie", "HTTPS 下设置 Secure；最长 8 小时"),
        ("后续请求核验会话与身份权限", "管理员、个人查询、授课课程分别验证"),
        ("本人资料或本人成绩查询", "学生请求不返回其他学生的成绩列表")
    ], "密码修改、退出与销户使相关会话失效")
    print("Generated UML, ranking and login diagrams in docs/report-assets")


if __name__ == "__main__":
    main()
