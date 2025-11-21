<script lang="ts">
  const { as = "button", children, ...attrs } = $props();
  let classes = $state(
    "border rounded p-1 bg-neutral-700 text-center text-neutral-300 hover:bg-neutral-600 active:bg-neutral-500 disabled:opacity-50 disabled:cursor-not-allowed"
  );

  $effect(() => {
    if (attrs.class) {
      classes += ` ${attrs.class}`;
      delete attrs.class;
    }
  });
</script>

{#if as === "div"}
  <div class={classes} {...attrs}>{@render children?.()}</div>
{:else if as === "a"}
  <span class={classes} {...attrs}>{@render children?.()}</span>
{:else}
  <button class={classes} type="button" {...attrs}
    >{@render children?.()}</button
  >
{/if}
