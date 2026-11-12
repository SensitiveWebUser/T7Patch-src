/**
 * The accessibility contracts of the component library, as tests.
 *
 * These exist because the defect they cover is invisible: the previous tab strip LOOKED correct
 * and rendered the right thing on screen. What was wrong - the selected tab being `disabled`, so
 * unreachable by keyboard and announced as unavailable - only shows up if something actually
 * presses a key. A rendered-markup snapshot would have passed.
 *
 * Needs a DOM, so `happy-dom` is a devDependency. If `bun test` starts failing at the import here,
 * that is the dependency having gone missing, not the components.
 */
import { test, expect } from 'bun:test'
import { act, useState } from 'react'
import Tabs, { Tab } from './Tabs.jsx'
import Checkbox from './Checkbox.jsx'
import Field from './Field.jsx'
import TextInput from './TextInput.jsx'
import { useDom } from '../testing/dom.js'

const { win, mount } = useDom()

function press(el, key) {
  act(() => {
    el.dispatchEvent(new win.KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }))
  })
}

function Harness() {
  const [page, setPage] = useState('basic')
  return (
    <Tabs value={page} onChange={setPage} label="Sections">
      <Tab id="basic" label="Basic">
        <input id="in-basic" defaultValue="" />
      </Tab>
      <Tab id="advanced" label="Advanced">
        <span>advanced</span>
      </Tab>
      <Tab id="off" label="Off" disabled>
        <span>off</span>
      </Tab>
    </Tabs>
  )
}

test('the selected tab is keyboard-reachable and never disabled', () => {
  const host = mount(<Harness />)
  const tabs = [...host.querySelectorAll('[role="tab"]')]
  const selected = tabs.find((t) => t.getAttribute('aria-selected') === 'true')

  expect(selected.disabled).toBe(false)
  expect(selected.tabIndex).toBe(0)
  // Roving tabindex: exactly one tab in the page's tab order.
  expect(tabs.map((t) => t.tabIndex)).toEqual([0, -1, -1])
})

test('arrow, Home and End move selection and skip a disabled tab', () => {
  const host = mount(<Harness />)
  const tabs = () => [...host.querySelectorAll('[role="tab"]')]
  const selectedIndex = () => tabs().findIndex((t) => t.getAttribute('aria-selected') === 'true')

  press(tabs()[0], 'ArrowRight')
  expect(selectedIndex()).toBe(1)
  expect(document.activeElement).toBe(tabs()[1])

  // Index 2 is disabled, so ArrowRight from 1 wraps past it to 0 rather than stopping on it.
  press(tabs()[1], 'ArrowRight')
  expect(selectedIndex()).toBe(0)

  press(tabs()[0], 'ArrowLeft')
  expect(selectedIndex()).toBe(1)

  // End lands on the last ENABLED tab.
  press(tabs()[1], 'End')
  expect(selectedIndex()).toBe(1)

  press(tabs()[1], 'Home')
  expect(selectedIndex()).toBe(0)
})

test('a disabled tab ignores a click instead of being unreachable', () => {
  const host = mount(<Harness />)
  const tabs = [...host.querySelectorAll('[role="tab"]')]

  expect(tabs[2].getAttribute('aria-disabled')).toBe('true')
  expect(tabs[2].disabled).toBe(false) // aria-disabled, not the native attribute
  act(() => tabs[2].dispatchEvent(new win.MouseEvent('click', { bubbles: true })))
  expect(tabs[0].getAttribute('aria-selected')).toBe('true')
})

test('panels stay mounted across a switch, so their state survives', () => {
  const host = mount(<Harness />)
  host.querySelector('#in-basic').value = 'half-typed'

  press(host.querySelector('[role="tab"]'), 'ArrowRight')

  const panels = [...host.querySelectorAll('[role="tabpanel"]')]
  expect(panels).toHaveLength(3)
  expect(panels[0].hasAttribute('hidden')).toBe(true)
  expect(panels[1].hasAttribute('hidden')).toBe(false)
  // Never unmounted, so the uncontrolled value is still in the DOM.
  expect(host.querySelector('#in-basic').value).toBe('half-typed')
})

test('every tab points at the panel it labels, and back', () => {
  const host = mount(<Harness />)
  // getElementById, not querySelector: React's useId emits ids like ":r6:", and a bare colon is
  // not a valid CSS selector. The ids are perfectly legal HTML - only the selector syntax objects.
  for (const tab of host.querySelectorAll('[role="tab"]')) {
    const panel = document.getElementById(tab.getAttribute('aria-controls'))
    expect(panel).not.toBeNull()
    expect(panel.getAttribute('role')).toBe('tabpanel')
    expect(panel.getAttribute('aria-labelledby')).toBe(tab.id)
  }
})

test('Checkbox writes indeterminate, which has no attribute to render', () => {
  const host = mount(<Checkbox label="Mixed" indeterminate checked={false} onValueChange={() => {}} />)
  const box = host.querySelector('input[type="checkbox"]')

  expect(box.indeterminate).toBe(true)
  expect(box.id).toBeTruthy()
  expect(host.querySelector('label').getAttribute('for')).toBe(box.id)
})

test('Field wires label, hint and error to ids that exist', () => {
  const host = mount(
    <Field label="Player name" hint="Shown in lobbies" error="Too long">
      <TextInput value="x" onValueChange={() => {}} />
    </Field>,
  )
  const input = host.querySelector('input')

  expect(host.querySelector('label').getAttribute('for')).toBe(input.id)
  expect(input.getAttribute('aria-invalid')).toBe('true')

  // aria-errormessage is ignored by every screen reader unless aria-invalid is set, and it must
  // point at a real element.
  const errorId = input.getAttribute('aria-errormessage')
  expect(document.getElementById(errorId).textContent).toBe('Too long')

  const hintId = input.getAttribute('aria-describedby')
  expect(document.getElementById(hintId).textContent).toBe('Shown in lobbies')
})

test('the Field error node exists before there is an error, or the live region never fires', () => {
  const host = mount(
    <Field label="Player name">
      <TextInput value="x" onValueChange={() => {}} />
    </Field>,
  )
  const region = host.querySelector('.field-error')

  expect(region).not.toBeNull()
  expect(region.getAttribute('aria-live')).toBe('polite')
  expect(region.textContent).toBe('')
  // Not referenced while valid: a description pointing at empty text is noise.
  expect(host.querySelector('input').getAttribute('aria-errormessage')).toBeNull()
})
