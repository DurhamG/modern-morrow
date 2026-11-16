// The cloud saves Worker. It answers every request with 404 until the requests in ui-rework/cloud-saves.md exist.
export default {
  async fetch(): Promise<Response> {
    return new Response("Not found", { status: 404 });
  },
} satisfies ExportedHandler;
