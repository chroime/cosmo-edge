import assert from 'node:assert/strict'
import * as Vue from 'vue'
import { mountComponent } from './helpers/mount_behavior_component.mjs'

// Exercise the real realtime player. Only the transport and browser timers are
// controlled: changing presentation must retain video elements and live viewers.
const hasClass = (node, name) => String(node.props.class || '').split(/\s+/).includes(name)
for (const protocol of ['httpflv', 'webrtc']) {
  const fits = Vue.reactive(['cover', 'cover'])
  const fullscreen = Vue.ref(null)
  const transports = [], timers = new Map(), calls = { request: [], stop: [], heartbeat: [] }
  let timerId = 0
  const schedule = callback => { const id = ++timerId; timers.set(id, callback); return id }
  const makeTransport = video => {
    const player = Vue.markRaw({ video, starts: 0, stops: 0, destroys: 0,
      play() { this.starts++ }, pause() { this.stops++ }, unload() {}, detachMediaElement() {},
      destroy() { this.destroys++ }, stop() { this.stops++ }
    })
    transports.push(player)
    return player
  }
  const flv = { isSupported: () => true, Events: { ERROR: 'error', SCRIPTDATA_ARRIVED: 'metadata' },
    createPlayer() {
      const listeners = new Map(), transport = makeTransport(null)
      Object.assign(transport, {
        attachMediaElement(video) {
          this.video = Vue.toRaw(video)
          video.buffered = { length: 0 }
          video.addEventListener = (event, fn) => listeners.set(event, fn)
          video.removeEventListener = (event, fn) => { if (listeners.get(event) === fn) listeners.delete(event) }
        },
        on() {}, off() {}, load() { listeners.get('canplay')?.() }
      })
      return transport
    }
  }
  const view = await mountComponent('views/box/bigScreen/components/flvVideo.vue', {
    mocks: {
      'flv.js': { default: flv },
      '@element-plus/icons-vue': Object.fromEntries(['FullScreen', 'ScaleToOriginal', 'Close', 'VideoCamera'].map(name => [name, name])),
      '@/utils/i18nResource': { resolveResourceAlgorithmName: item => item.algorithmName },
      '@/utils/whepPlayer': { createWhepPlayer(options) {
        const transport = makeTransport(Vue.toRaw(options.video))
        transport.start = () => { transport.starts++; options.onConnected() }
        return transport
      } }
    },
    globals: {
      localStorage: { getItem: () => null },
      window: { location: { hostname: 'fixture.invalid', protocol: 'http:' }, RTCPeerConnection: class {} },
      setTimeout: schedule, clearTimeout: id => timers.delete(id),
      setInterval: schedule, clearInterval: id => timers.delete(id),
      console: { log() {}, warn: (...args) => assert.fail(args.join(' ')), error: (...args) => assert.fail(args.join(' ')) }
    },
    api: {
      async boxRequestLiveStream(params) {
        calls.request.push(params)
        return { resData: { stream: { protocol, flvUrl: '/fixture/live.flv', webrtcUrl: '/fixture/whep', httpPort: 8080, rtcApiPort: 1985, keepAliveInterval: 10 } } }
      },
      async boxStreamKeepAlive(params) { calls.heartbeat.push(params) },
      async boxStreamStop(params) { calls.stop.push(params) }
    },
    message: { error: message => assert.fail(message) },
    wrap: Player => ({ setup: () => () => Vue.h('fixture-wall', {}, fits.map((fit, index) => Vue.h(Player, {
      key: index, index, channelId: `channel-${index}`, cameraName: `Camera ${index}`, runAlgorithmId: '', taskList: [],
      fitMode: fit, 'onUpdate:fitMode': value => { fits[index] = value },
      isFullScreen: fullscreen.value === index ? index : null,
      onFullScreen: event => { fullscreen.value = event.isFullScreen ? index : null }
    }))) })
  })
  const flush = async () => { for (let step = 0; step < 12; step++) await view.settle() }
  const media = () => view.all(node => node.type === 'video')
  const fitButtons = () => view.all(node => hasClass(node, 'video-fit'))
  try {
    await flush()
    const originalVideo = media()
    assert.equal(originalVideo.length, 2)
    assert.equal(calls.request.length, 2)
    assert.equal(transports.length, 2)
    const heartbeatCount = calls.heartbeat.length
    assert.deepEqual(media().map(node => node.props.style.objectFit), ['cover', 'cover'])
    assert.ok(fitButtons().every(button => button.props['aria-label'] === 'event.fitContain'))
    for (const index of [0, 0, 0, 1, 1]) {
      fitButtons()[index].props.onClick()
      await flush()
    }
    assert.deepEqual([...fits], ['contain', 'cover'], 'each window owns its display choice')
    assert.deepEqual(media().map(node => node.props.style.objectFit), ['contain', 'cover'])
    assert.equal(fitButtons()[0].props['aria-label'], 'event.fitCover')
    assert.equal(fullscreen.value, null, 'fit control does not request fullscreen')
    const fullButton = view.all(node => node.type === 'button' && node.props['aria-label'] === 'event.fullscreen')[0]
    fullButton.props.onClick()
    await flush()
    assert.equal(fullscreen.value, 0)
    assert.deepEqual([...fits], ['contain', 'cover'], 'fullscreen keeps the chosen display mode')
    view.all(node => node.type === 'button' && node.props['aria-label'] === 'event.exitFullscreen')[0].props.onClick()
    await flush()
    assert.equal(fullscreen.value, null)
    assert.ok(media().every((node, index) => node === originalVideo[index]), 'presentation retains the actual video element')
    assert.equal(calls.request.length, 2, 'fit and fullscreen never acquire replacement viewers')
    assert.equal(calls.stop.length, 0, 'fit and fullscreen never release live viewers')
    assert.equal(calls.heartbeat.length, heartbeatCount, 'fit and fullscreen do not restart heartbeat')
    assert.equal(transports.length, 2, 'fit and fullscreen do not create transports')
    assert.ok(transports.every(player => player.starts === 1 && player.stops === 0 && player.destroys === 0))
  } finally {
    view.unmount()
    await flush()
  }
  assert.equal(calls.stop.length, 2, 'normal unmount releases each viewer once')
  assert.equal(timers.size, 0, 'normal cleanup clears all viewer timers')
}
console.log('Realtime display checks passed: independent fit modes, retained video/FLV/WebRTC viewers and heartbeat, fullscreen separation, normal cleanup. Transports are controlled; device layout remains a separate check.')
