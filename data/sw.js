// Service Worker for 智能老人跌倒监护系统
const CACHE_NAME = 'esp32-fall-detection-v2';
const STATIC_CACHE = 'static-cache-v2';
const DYNAMIC_CACHE = 'dynamic-cache-v2';

// 需要缓存的静态资源
const STATIC_ASSETS = [
  '/',
  '/index.html',
  '/modern.css'
];

// 需要缓存的第三方资源
const THIRD_PARTY_ASSETS = [
  'https://cdn.bootcdn.net/ajax/libs/vue/3.2.47/vue.global.prod.min.js',
  'https://cdn.bootcdn.net/ajax/libs/element-plus/2.3.1/index.full.min.js',
  'https://cdn.bootcdn.net/ajax/libs/element-plus/2.3.1/index.min.css',
  'https://cdn.bootcdn.net/ajax/libs/Chart.js/3.9.1/chart.min.js',
  'https://cdn.bootcdn.net/ajax/libs/three.js/r128/three.min.js'
];

// 所有需要缓存的资源
const ASSETS_TO_CACHE = [...STATIC_ASSETS, ...THIRD_PARTY_ASSETS];

// 默认离线页面模板（简化版本）
const OFFLINE_PAGE_TEMPLATE = `
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>离线模式 - 智能老人跌倒监护系统</title>
  <style>
    body {
      font-family: "PingFang SC", "SF Pro Display", "Helvetica Neue", Arial, sans-serif;
      margin: 0;
      padding: 0;
      background: #0A0E1A;
      color: #E0E0E0;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      text-align: center;
    }
    .container {
      max-width: 600px;
      padding: 20px;
      background: rgba(24, 29, 49, 0.85);
      backdrop-filter: blur(12px);
      border-radius: 12px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
      border: 1px solid rgba(255, 255, 255, 0.08);
    }
    h1 {
      font-size: 24px;
      margin-bottom: 20px;
      background: linear-gradient(45deg, #409EFF, #61dafb);
      -webkit-background-clip: text;
      background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    p {
      margin-bottom: 15px;
      line-height: 1.5;
    }
    button {
      background: linear-gradient(45deg, #409EFF, #61dafb);
      border: none;
      color: white;
      padding: 10px 20px;
      border-radius: 8px;
      cursor: pointer;
      font-weight: 500;
      transition: all 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
    }
    .status {
      margin-top: 20px;
      font-size: 14px;
      color: #AAAAAA;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>智能老人跌倒监护系统</h1>
    <p>您当前处于离线状态，无法连接到设备。</p>
    <p>请检查您的网络连接或设备状态，然后重试。</p>
    <button onclick="tryReconnect()">尝试重新连接</button>
    <div class="status" id="status"></div>
  </div>
  <script>
    function tryReconnect() {
      const statusEl = document.getElementById('status');
      statusEl.textContent = '正在尝试重新连接...';
      
      setTimeout(() => {
        window.location.reload();
      }, 1500);
    }
    
    // 检测在线状态
    window.addEventListener('online', () => {
      window.location.reload();
    });
  </script>
</body>
</html>
`;

// 安装Service Worker
self.addEventListener('install', event => {
  console.log('[Service Worker] 正在安装');
  
  // 跳过等待，立即激活
  self.skipWaiting();
  
  event.waitUntil(
    Promise.all([
      // 缓存静态资源
      caches.open(STATIC_CACHE).then(cache => {
        console.log('[Service Worker] 缓存静态资源');
        return cache.addAll(STATIC_ASSETS);
      }),
      
      // 缓存第三方资源
      caches.open(STATIC_CACHE).then(cache => {
        console.log('[Service Worker] 缓存第三方资源');
        return cache.addAll(THIRD_PARTY_ASSETS);
      }),
      
      // 创建并存储离线页面
      caches.open(STATIC_CACHE).then(cache => {
        return cache.put(
          new Request('/offline.html'),
          new Response(OFFLINE_PAGE_TEMPLATE, {
            headers: {'Content-Type': 'text/html; charset=utf-8'}
          })
        );
      })
    ])
  );
});

// 激活Service Worker
self.addEventListener('activate', event => {
  console.log('[Service Worker] 激活');
  
  // 立即控制页面
  self.clients.claim();
  
  event.waitUntil(
    // 清理旧缓存
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (
            cacheName !== STATIC_CACHE && 
            cacheName !== DYNAMIC_CACHE &&
            cacheName !== CACHE_NAME
          ) {
            console.log('[Service Worker] 删除旧缓存:', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
});

// 拦截请求并提供缓存响应
self.addEventListener('fetch', event => {
  // 跳过非GET请求和浏览器扩展请求
  if (event.request.method !== 'GET' || event.request.url.startsWith('chrome-extension://')) {
    return;
  }

  // WebSocket请求不处理
  if (event.request.url.includes('/ws')) {
    return;
  }

  event.respondWith(
    // 优先使用网络请求，失败时再使用缓存
    fetch(event.request)
      .then(response => {
        // 复制响应以便缓存
        const responseClone = response.clone();
        
        // 只缓存成功的响应
        if (response.status === 200) {
          caches.open(DYNAMIC_CACHE).then(cache => {
            cache.put(event.request, responseClone);
          });
        }
        
        return response;
      })
      .catch(() => {
        // 网络请求失败时使用缓存
        return caches.match(event.request)
          .then(cachedResponse => {
            // 如果有缓存则返回缓存
            if (cachedResponse) {
              return cachedResponse;
            }
            
            // 对于HTML请求，返回离线页面
            if (event.request.headers.get('accept').includes('text/html')) {
              return caches.match('/offline.html');
            }
            
            // 其他请求无法处理时显示错误
            return new Response('网络离线，无法加载资源', {
              status: 503,
              statusText: 'Service Unavailable',
              headers: new Headers({
                'Content-Type': 'text/plain'
              })
            });
          });
      })
  );
});

// 处理推送通知
self.addEventListener('push', event => {
  console.log('[Service Worker] 收到推送消息:', event.data.text());
  
  const options = {
    body: event.data.text(),
    icon: '/icon-192x192.png',
    badge: '/badge-72x72.png',
    vibrate: [100, 50, 100],
    data: {
      dateOfArrival: Date.now(),
      primaryKey: 1
    },
    actions: [
      {action: 'view', title: '查看详情'}
    ]
  };
  
  event.waitUntil(
    self.registration.showNotification('智能老人跌倒监护系统', options)
  );
});

// 处理通知点击
self.addEventListener('notificationclick', event => {
  event.notification.close();
  
  if (event.action === 'view') {
    // 打开应用主页
    event.waitUntil(
      clients.openWindow('/')
    );
  }
});

// 周期性同步（如果支持）
self.addEventListener('periodicsync', event => {
  if (event.tag === 'check-device-status') {
    event.waitUntil(checkDeviceStatus());
  }
});

// 检查设备状态
async function checkDeviceStatus() {
  try {
    const response = await fetch('/api/status');
    const data = await response.json();
    
    if (data.fallenState) {
      // 如果检测到跌倒状态，显示通知
      self.registration.showNotification('跌倒警报', {
        body: '系统检测到老人可能跌倒，请立即查看',
        icon: '/icon-192x192.png',
        vibrate: [200, 100, 200, 100, 200],
        requireInteraction: true
      });
    }
  } catch (error) {
    console.error('[Service Worker] 检查设备状态失败:', error);
  }
}

// 后台同步
self.addEventListener('sync', event => {
  if (event.tag === 'sync-device-status') {
    event.waitUntil(
      // 这里可以添加设备状态同步逻辑
      console.log('同步设备状态')
    );
  }
});

// 推送通知
self.addEventListener('push', event => {
  const data = event.data.json();
  
  const options = {
    body: data.body || '检测到新的跌倒事件',
    icon: '/icon-192x192.png',
    badge: '/badge-96x96.png',
    vibrate: [100, 50, 100],
    data: {
      url: data.url || '/'
    }
  };
  
  event.waitUntil(
    self.registration.showNotification(
      data.title || '跌倒检测警报',
      options
    )
  );
});

// 点击通知
self.addEventListener('notificationclick', event => {
  event.notification.close();
  
  event.waitUntil(
    clients.matchAll({type: 'window'})
      .then(windowClients => {
        if (windowClients.length > 0) {
          return windowClients[0].focus();
        }
        return clients.openWindow(event.notification.data.url);
      })
  );
}); 