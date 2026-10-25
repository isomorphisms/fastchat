# Install and start a conversation

This is a test candidate. Keep the existing application installed: a normal
replacement install preserves its private history and settings. An uninstall
deletes them. A signature conflict requires investigation, not an uninstall.

## Choose an APK

- MIRO A1: `armeabi-v7a`.
- MIRO C67: `arm64-v8a` when a qualified companion artifact is available.

Download only an artifact whose source, SHA-256, package, version, ABI and signer
match its producer receipt. Hosted checks and artifacts are linked from
[the existing candidate](https://github.com/isomorphisms/fastchat/pull/17).
ARM32 running on C67 would establish ARM32 compatibility only.

The package remains `org.isomorphisms.fastchat.diskstreaming.arena` and the
launcher label remains **FastChat live disk**. Its public development signer
certificate SHA-256 is
`aa9151e3922fa4795987c3655bc7aa4712620f1d94c24a2075acd7b29d8ac86e`.
This is a sideloaded test signer, not a store production signer.

## Configure a provider

Open **Setup** beside the conversation status. Fill in:

| Field | Value |
| --- | --- |
| HTTPS origin | Your API provider's origin, such as `https://api.openai.com`; omit the endpoint path. |
| Endpoint path | The provider's documented path, such as `/v1/chat/completions` or `/v1/responses`. |
| Model | The exact model ID available to your API account. |
| Format | `chat-completions` or `responses`, matching the chosen endpoint. |
| Authorization header | Usually `Authorization: Bearer YOUR_API_KEY`; an empty field clears stored authorization. |
| CA | `system` uses the phone's system root certificates. An absolute PEM path supports an explicitly configured private CA. |

Tap **Save**, then type a short message and tap **Send**. The editor and keyboard
handle ordinary Unicode editing, selection and paste. Provider changes are
blocked during generation; stop first. **Back** discards unsaved setup edits.
The credentials panel excludes screenshots and masks the authorization field.

This is API-client access. ChatGPT consumer-account login and subscription
entitlements are separate; this candidate does not implement that login.
Live-provider compatibility still requires an actual provider receipt.

Without any provider configuration, the heading explicitly identifies the
credential-free **test responder**. `:long`, `:lost` and `:fail` exercise long
reading, uncertain delivery and explicit failure. Invalid saved configuration
disables submission until repaired in Setup; it does not switch to the demo.

## Read and return

Use Older/Newer/Latest for turns, Previous/Next/Start for pages, or swipe
vertically. Copy opens Android text selection for the currently displayed
bounded page. Stop requests cancellation. Retry explicitly resubmits an
uncertain request; the provider may already have received the original.

Submitted history is durable. Drafts are checkpointed on change at roughly
one-second intervals and when leaving the app or opening Setup. A sudden kill
can lose edits since the last completed checkpoint. Rotation also uses Android
saved state. Ordinary signed replacement updates preserve the private store.

## Current limits

- One active request and one canonical conversation store.
- Prompts: 1,024 UTF-8 bytes. Saved drafts: 4,096 UTF-16 units.
- The serialized provider context is bounded to 8,192 bytes, including durable
  prior turns. Excess context refuses explicitly before POST; history is never
  silently dropped or summarized.
- Reader windows are bounded to 4,096 bytes. Copy selects one displayed page.
- Markdown support covers basic headings and fenced code, not full CommonMark.
- Provider records, nesting, JSON tokens and tool-event sizes are bounded.
  Tools are retained as events and are not executed. Unknown or oversized
  provider events fail explicitly; arbitrary provider compatibility is unclaimed.
- No consumer ChatGPT login, automatic job dispatch or HTTP/3 production path.
- Earlier readers may reject the new nonempty attempt-identity payload. Keep a
  current signed version; downgrade compatibility is unclaimed.

Physical keyboard, lifecycle, system-trust and live-provider acceptance remain
required before a release is advertised as usable on either phone.
