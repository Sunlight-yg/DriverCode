<?php 
session_start();
if(is_null(@$_SESSION['username']))
	$_SESSION['username'] = null;
?>
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="context-type" context="text/html" charset="utf-8" />
    <title>阳光</title>
    <link rel="shortcut icon" href="img/sunlight.png" />
    <link rel="stylesheet" type="text/css" href="css/index.css" />
    <?php if(!is_null($_SESSION['username'])) echo "<link rel='stylesheet' type='text/css' href='css/index.icon.css' />" ?>
</head>
<body>
    <div class="top">
        <div class="outNick">
            <div class="nick">
                首页
            </div>
        </div>
        <div class="outNav">
            <div class="nav">
                <ul>
                    <li>
                        <a href="index.php">首页</a>
                    </li>
                    <li>
                        <a class="photos" href="photos.php">相册</a>
                    </li>
                    <li class="about">
                        <a href="about.php">关于</a>
                    </li>
                    <li class="icon">
                        <a <?php if(!is_null($_SESSION['username'])){ echo "href='userText.php'"; }else{ echo "href='login.html'";  } echo '>'; if(!is_null($_SESSION['username'])){ echo "<img src='img/icon.png' height='35' />"; }else{ echo "登陆"; } ?>
                </a>
                    </li><?php if(!is_null($_SESSION['username'])) echo "<li class='exit'><a href='php/exit.php'>退出</a></li>" ?>
                </ul>
            </div>
        </div>
    </div>
    <div class="content">
        <h2 class="h">【米柚科技说】小米MIX 2S为什么值得买？</h2>
        <img class="mi" src="img/mix2s.png" width="90%" />
        <h2 class="h">正文</h2>
        <h2 class="h">
            小米MIX2S对比MIX2，提升不止一点点
        </h2>
        <div class="main">
            <p>
                几年前，横空出世的小米凭借“性价比”一招就把手机市场搅得天翻地覆。而随着近两年的消费升级，所带来的不仅仅是消费者对产品的要求，也带来对品牌的看重。因此各品牌都在寻求拔高自己品牌形象的方式。有的品牌选择请顶级流量的代言人，而小米这样依靠互联网起家的品牌，则选择了用产品说话，于是小米MIX系列应运而生。
            </p>
            <img class="img" src="img/1.jpg" />
            <p>从小米MIX的“全面屏概念手机”，到小米MIX2“全面屏2.0”，MIX系列逐渐去概念化，从概念手机过度到顶级旗舰机的方向。而作为顶级旗舰机，小米MIX2却不够完全接地气，比如现今手机上最重要的拍照，一直是MIX系列的短板。做概念机时可以任性，想要成为实用的旗舰机，则需要在各方面都不能有明显的短板。因此可以看到，在小米MIX 2S上，双摄的加入、去掉全陶瓷版，正是传达了从概念机彻底向旗舰机的转换。在这样的转换中，小米MIX 2S究竟实力如何，这就是我们今天需要弄清的问题。</p>
            <img class="img" src="img/2.jpg" />
            <h3>硬件配置：</h3>
            <p>小米MIX 2S从命名上就很有趣，手机以“S”为后缀的产品大家都见得不少，一般来说都是上一代产品的改款，而非一个比较明显的换代产品。下面我们就通过表格，来看看此次小米MIX 2S在哪方面有所提升。</p>
            <img class="img" src="img/3.jpg" />
            <h3>外观设计：延续MIX2的设计</h3>
            <p>
                从表格中可以看出，小米MIX 2S相比小米MIX2在实用性的提升上比较明显：IMX363传感器1．4μm的单个像素相比上一代1．25μm有了比较明显的提升，Dual PD全像素双核对焦比PDAF也有了明显的进步。相比上一代在拍照方面所带来的提升，对于普通用户更加实用。而骁龙845的加入，使得小米MIX 2S在旗舰气质上也堪称顶级。
            </p>
            <p>
                小米MIX 2S整体延续了先前的“全面屏 2．0”设计理念，如果仅从正面来看，小米MIX 2S与小米MIX 2没有任何区别，依旧搭载一块5．99英寸18：9 FHD＋全面屏，四周圆角切割，采用与上代产品一致的COF封装工艺，因此其下巴部分的宽度并没有缩减。前置相机位于机身右下角，默认是略显尴尬的仰视角度拍摄，实测无碍人脸识别，但自拍需要翻转手机。顶部为导管式隐藏听筒，两侧隐藏了超声波距离传感器，这些都是MIX 2设计的直接传承。
            </p>
            <img class="img" src="img/4.jpg" />
            <h3>AI拍照 DxOMark拍照评分101分</h3>
            <p>小米MIX 2在背部设计上有了较大幅度的改动，指纹识别居中，双摄位于左上角，造型与iPhone X造型极其相似，同为垂直排列，并略突出于机身，最上为1200万像素广角主摄，最下为1200万像素长焦副摄，中间为闪光灯。整体来看，小米MIX 2S延续了“全面屏2．0”的设计理念，并无太多惊喜。就过往的MIX系列产品而言，不论是初代MIX还是MIX 2，其成像表现都有些不尽如人意，夜拍更是明显的短板。因此在小米MIX 2S上，小米带来了更多积极的改良措施，包括长焦＋广角的双摄搭配，影像传感器的升级、AI算法的引入等，下面我们就来详细说明。</p>
            <img class="img" src="img/5.jpg" />
            <h3>硬件测试：国产首发骁龙845，性能无需置疑</h3>
            <p>骁龙835采用第二代三星10nm LPP FINFET工艺，4个性能核心＋四个能效核心，大核主频为恐怖的2．8GHz，而小核主频则为1．8GHz。强悍的硬件配置可以说已经成为小米手机的基因，尤其对于旗舰机。此次，小米MIX 2S作为骁龙845国产手机的首发之作，工程难度上也绝非买个芯片焊在PCB上那么简单。甚至雷军都在微博科普：在手机研发时，高通芯片的研发完成度也比较低，因此需要终端厂商共同参与测试并协助修复各种BUG，小米甚至在高通总部圣地亚哥设立了研发部门，以保证骁龙845的顺利首发。</p>
            <img class="img" src="img/6.jpg" />
            <p>GPU方面，骁龙845搭载Adreno 630，主频与骁龙835一致，为710MHz，根据官方数据，Adreno 630相比540在性能上提升30％（比540对比530的提升还要高一点），功耗降低了30％，显示接口数据吞吐量提升2．5倍，Adreno同样支持OpenGL ES 3．2、OpenCL 2．0、Vulkan2、DxNext各种标准，当然，对于时下大热的AI，骁龙845也并未缺席。而与麒麟970采用独立NPU的方式不同，骁龙845则是SoC中的Hexagon 685 DSP、CPU和GPU来完成AI在硬件方面的计算。具体来说，Hexagon 685 DSP支持8位定点运算，负责处理涉及向量数学的应用工作负载；而对于对浮点精度有要求的负载，高通则放在性能强悍的Adreno 630上做16位浮点精度运算；CPU则优化了8位定点精度的支持。</p>
            <img class="img" src="img/7.jpg" />
            <h3>无线快充</h3>
            <p>搭载的无线快充，对于三星苹果用户可能并不陌生，Phone8系列和iPhone X也支持无线快充功能，但是也是第三方无线充电器。此次小米国内首发无线快充，无疑又是技术上的再次突破。而且该快充还支持Phone8系列和iPhone X等机型，最关键的是价格为99元，苹果、三星的无心充电板都在500元上下可谓是良心大大的。</p>
            <p><strong>MIUI一直是安卓第三方定制OS的佼佼者，无论从功能本地化还是系统流畅度，去年的MIUI 9都备受好评。在小米MIX 2S上，除去这些基础的功能之外，小米MIX 2S也打起AI牌，将语音助手“小爱同学”完全移植到MIUI中。而其实无论作为手机本身的卖点，还是对小米已经构建出庞大智能家居生态链的终端控制，这一步其实都并不意外。</strong></p>
            <p><strong>在AR方面，此次小米作为第一批支持谷歌ARCore的产品。AR技术随着手机性能的不断发展，已经逐渐超过VR，成为手机在未来最重要的技术领域。而谷歌ARCore则通过运动追踪技术、环境理解技术、光照强度估测技术三合一，整合手机对当前环境的理解。MIX 2S作为首批支持ARCore的产品，也在手机中内置了名为The Machines的游戏。在摄像头对准身边任何一块平整的表面上，都可以在屏幕中投影出游戏画面。</strong></p>
            <p class="footp">————转自MIUI论坛</p>
        </div>
    </div>
</body>
</html>